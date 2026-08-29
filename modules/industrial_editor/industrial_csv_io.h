#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"

class IndustrialProject;

// Export devices to CSV (no ScanGroup column).
Error industrial_export_devices_csv(Ref<IndustrialProject> p_project, const String &p_path);

// Export tags to CSV.
Error industrial_export_tags_csv(Ref<IndustrialProject> p_project, const String &p_path);

// Import devices from CSV.
Error industrial_import_devices_csv(Ref<IndustrialProject> p_project, const String &p_path);

// Import tags from CSV.
Error industrial_import_tags_csv(Ref<IndustrialProject> p_project, const String &p_path);
