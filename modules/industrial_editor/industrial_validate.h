#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/array.h"

class IndustrialProject;

// Extended project validation: driver-specific checks, address syntax,
// parameter completeness. Returns an array of error message strings.
Array industrial_validate_project(Ref<IndustrialProject> p_project);
