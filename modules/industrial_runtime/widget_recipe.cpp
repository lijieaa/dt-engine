#include "widget_recipe.h"

#include "core/object/class_db.h"
#include "core/variant/array.h"

void WidgetRecipe::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_version"), &WidgetRecipe::get_version);
	ClassDB::bind_method(D_METHOD("configure", "cfg"), &WidgetRecipe::configure);
	ClassDB::bind_method(D_METHOD("list_entries"), &WidgetRecipe::list_entries);
	ClassDB::bind_method(D_METHOD("build_write_sequence", "entry_name"), &WidgetRecipe::build_write_sequence);
	ClassDB::bind_method(D_METHOD("clear"), &WidgetRecipe::clear);
}

String WidgetRecipe::get_version() const {
	return "0.1.0";
}

bool WidgetRecipe::configure(const Dictionary &p_cfg) {
	entries.clear();
	if (!p_cfg.has("entries")) {
		return false;
	}
	const Variant entries_v = p_cfg["entries"];
	if (entries_v.get_type() != Variant::ARRAY) {
		return false;
	}
	Array raw_entries = entries_v;
	for (int i = 0; i < raw_entries.size(); i++) {
		const Variant e = raw_entries[i];
		if (e.get_type() != Variant::DICTIONARY) {
			entries.clear();
			return false;
		}
		Dictionary d = e;
		if (!d.has("name") || !d.has("writes") || d["writes"].get_type() != Variant::ARRAY) {
			entries.clear();
			return false;
		}
		Entry entry;
		entry.name = String(d["name"]);
		Array raw_writes = d["writes"];
		for (int j = 0; j < raw_writes.size(); j++) {
			const Variant w = raw_writes[j];
			if (w.get_type() != Variant::DICTIONARY) {
				entries.clear();
				return false;
			}
			Dictionary wd = w;
			if (!wd.has("tag")) {
				entries.clear();
				return false;
			}
			WriteStep step;
			step.tag = String(wd["tag"]);
			step.value = wd.has("value") ? wd["value"] : Variant();
			entry.writes.push_back(step);
		}
		entries.push_back(entry);
	}
	return true;
}

Array WidgetRecipe::list_entries() const {
	Array result;
	for (int i = 0; i < entries.size(); i++) {
		result.push_back(entries[i].name);
	}
	return result;
}

Array WidgetRecipe::build_write_sequence(const String &p_entry_name) const {
	Array result;
	for (int i = 0; i < entries.size(); i++) {
		if (entries[i].name == p_entry_name) {
			for (int j = 0; j < entries[i].writes.size(); j++) {
				Dictionary step;
				step["tag"] = entries[i].writes[j].tag;
				step["value"] = entries[i].writes[j].value;
				step["order"] = j;
				result.push_back(step);
			}
			break;
		}
	}
	return result;
}

void WidgetRecipe::clear() {
	entries.clear();
}