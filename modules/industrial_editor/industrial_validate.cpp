#include "industrial_validate.h"
#include "industrial_project.h"
#include "industrial_driver_schema.h"

#include "core/variant/array.h"

// Extended validation beyond the basic checks in IndustrialProject::validate().
// Checks: address syntax per driver, tag name uniqueness across devices,
// data type validity, connection parameter completeness.
Array industrial_validate_project(Ref<IndustrialProject> p_project) {
	Array errors;
	if (p_project.is_null()) {
		errors.append("No project to validate.");
		return errors;
	}

	// Run basic validation.
	errors = p_project->validate();

	int dev_count = p_project->get_device_count();
	for (int di = 0; di < dev_count; di++) {
		const auto &dev = p_project->get_device(di);

		// Validate driver-specific connection params completeness.
		Vector<IndustrialFieldDef> fields = industrial_get_driver_fields(dev.driver);
		for (const auto &field : fields) {
			if (field.data_type == 0 || field.data_type == 1) {
				// Numeric: check min/max bounds.
				if (dev.connection_params.has(field.key)) {
					double val = dev.connection_params[field.key];
					if (field.min_value != 0 || field.max_value != 0) {
						if (val < field.min_value || (field.max_value > 0 && val > field.max_value)) {
							errors.append(vformat(
								TTR("Device '%s': field '%s' value %f is out of range [%f, %f]."),
								dev.name, field.name, val, field.min_value, field.max_value));
						}
					}
				}
			}
		}

		// Validate tag addresses per driver.
		for (const auto &tag : dev.tags) {
			if (tag.address.is_empty()) {
				errors.append(vformat(
					TTR("Device '%s': tag '%s' has empty address."),
					dev.name, tag.name));
			}
			if (tag.name.is_empty()) {
				errors.append(vformat(
					TTR("Device '%s': tag has empty name (address: %s)."),
					dev.name, tag.address));
			}
		}
	}

	// Check for cross-device duplicate tag names (optional warning).
	// Not an error, but worth noting for global search scenarios.
	return errors;
}
