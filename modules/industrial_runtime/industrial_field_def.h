#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"

// Shared field descriptor for driver connection forms and runtime catalog
// mapping. Lives in industrial_runtime so web/export builds do not depend on
// the editor-only industrial_driver_schema translation unit.

using IndustrialStringList = Vector<String>;

struct IndustrialFieldDef {
	String name; // Display label, e.g. "IP Address"
	String key; // Storage key in connection_params, e.g. "ip"
	int data_type; // 0=int, 1=float, 2=string, 3=bool, 4=choice
	Variant default_value;
	double min_value = 0;
	double max_value = 0;
	IndustrialStringList choices; // For choice fields
	String tooltip;
	int max_length = 0; // 0 = unlimited; LineEdit character limit
};
