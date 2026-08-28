#pragma once

#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"
#include "industrial_driver_schema.h"
#include "industrial_project.h"
#include "scene/gui/box_container.h"
#include "scene/gui/control.h"

// Build one labeled param row; registers widget in r_widgets[key].
void industrial_add_param_row(VBoxContainer *p_container, const IndustrialFieldDef &p_field,
		const Variant &p_value, HashMap<String, Control *> &r_widgets, bool p_translate_label = true);

// Read a single widget value (data_type from IndustrialFieldDef).
Variant industrial_read_param_widget(Control *p_widget, int p_data_type);

// Deduplicate catalog fields (last wins) and drop hidden keys.
Vector<IndustrialFieldDef> industrial_filter_driver_fields(const Vector<IndustrialFieldDef> &p_fields,
		const char *const *p_hidden_keys, int p_hidden_count);

// Collect all param widget values into a Dictionary.
void industrial_collect_param_widgets(const HashMap<String, Control *> &p_widgets, Dictionary &r_params);

// Backfill schema defaults for keys not present in r_params.
void industrial_backfill_param_defaults(int p_driver_idx, Dictionary &r_params,
		const char *const *p_skip_keys, int p_skip_count);
