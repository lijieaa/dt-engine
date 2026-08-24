#pragma once

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include "core/templates/vector.h"

// Field choice lists: a Vector of String. Typedef kept for readability across
// the schema/form code; concrete storage is Godot's Vector<String> which
// supports push_back from string literals via implicit String conversion.
using StringList = Vector<String>;

// A single driver-defined field descriptor for dynamic form generation.
struct IndustrialFieldDef {
	String name;          // Display label, e.g. "IP Address"
	String key;           // Storage key in connection_params, e.g. "ip"
	int data_type;        // 0=int, 1=float, 2=string, 3=bool, 4=choice
	Variant default_value;
	double min_value = 0;
	double max_value = 0;
	StringList choices;   // For choice fields
	String tooltip;
	int max_length = 0;   // 0 = unlimited; LineEdit 字符上限
};

// Static fallback metadata for a single driver, used when the Go runtime
// backend is unreachable.  Mirrors the JSON returned by GET /api/v1/drivers.
struct DriverMeta {
	const char *display_name;
	const char *driver_key;
	const char *vendor;
	const char *interface;
	const char *addressing_mode;
};

// Returns the field definitions for the given driver.
// Indexes align with the Go backend's GET /api/v1/drivers catalog.
Vector<IndustrialFieldDef> industrial_get_driver_fields(int p_driver);

// Returns human-readable driver name.
String industrial_get_driver_name(int p_driver);

// Returns human-readable data type name.
String industrial_get_data_type_name(int p_type);

// Returns the list of all data type names (for dropdown).
StringList industrial_get_data_type_names();

// Returns the list of all driver names (for dropdown).
StringList industrial_get_driver_names();

// Returns the list of S7 address type names (IB, IW, ID, ... DBx_String).
StringList industrial_get_s7_address_types();

// Returns the static fallback metadata for the given driver, or nullptr.
const DriverMeta *industrial_get_driver_meta(int p_driver);

// Returns the fallback driver key (e.g. "siemens_s7"), or empty string.
String industrial_get_driver_key(int p_driver);

// Returns the total driver count (max of backend and fallback).
int industrial_get_driver_count();
