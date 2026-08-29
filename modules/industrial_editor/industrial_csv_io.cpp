#include "industrial_csv_io.h"
#include "industrial_project.h"
#include "industrial_driver_schema.h"

#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"

namespace {

bool csv_header_has_scan_group(const String &p_header) {
	const Vector<String> cols = p_header.split(",");
	for (int i = 0; i < cols.size(); i++) {
		if (cols[i].strip_edges().unquote() == "ScanGroup") {
			return true;
		}
	}
	return false;
}

Error reject_scan_group_csv_header(const String &p_header) {
	if (csv_header_has_scan_group(p_header)) {
		ERR_PRINT(TTR("Obsolete CSV column ScanGroup is not allowed."));
		return ERR_INVALID_DATA;
	}
	return OK;
}

} // namespace

// Export devices to CSV.
// Columns: Name, Description, Driver, Enabled, ConnectionParams(JSON), TagCount
Error industrial_export_devices_csv(Ref<IndustrialProject> p_project, const String &p_path) {
	if (p_project.is_null()) {
		return ERR_INVALID_PARAMETER;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		return ERR_FILE_CANT_WRITE;
	}

	f->store_line("Name,Description,Driver,Enabled,ConnectionParams,TagCount");

	int count = p_project->get_device_count();
	for (int i = 0; i < count; i++) {
		const auto &dev = p_project->get_device(i);
		String csv_line;
		csv_line += "\"" + dev.name.replace("\"", "\"\"") + "\",";
		csv_line += "\"" + dev.description.replace("\"", "\"\"") + "\",";
		csv_line += industrial_get_driver_name(dev.driver) + ",";
		csv_line += dev.enabled ? "true" : "false";
		csv_line += ",";
		String params_json = JSON::stringify(dev.connection_params);
		csv_line += "\"" + params_json.replace("\"", "\"\"") + "\",";
		csv_line += itos((int)dev.tags.size());
		f->store_line(csv_line);
	}

	f->close();
	return OK;
}

// Export tags to CSV.
// Columns: Device, Address, Name, DataType, Writable, Scale, Unit
Error industrial_export_tags_csv(Ref<IndustrialProject> p_project, const String &p_path) {
	if (p_project.is_null()) {
		return ERR_INVALID_PARAMETER;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		return ERR_FILE_CANT_WRITE;
	}

	f->store_line("Device,Address,Name,DataType,Writable,Scale,Unit");

	int dev_count = p_project->get_device_count();
	for (int di = 0; di < dev_count; di++) {
		const auto &dev = p_project->get_device(di);
		for (const auto &tag : dev.tags) {
			String csv_line;
			csv_line += "\"" + dev.name.replace("\"", "\"\"") + "\",";
			csv_line += "\"" + tag.address.replace("\"", "\"\"") + "\",";
			csv_line += "\"" + tag.name.replace("\"", "\"\"") + "\",";
			csv_line += industrial_get_data_type_name(tag.data_type) + ",";
			csv_line += tag.writable ? "true" : "false";
			csv_line += ",";
			csv_line += String::num_real(tag.scale) + ",";
			csv_line += "\"" + tag.unit.replace("\"", "\"\"") + "\"";
			f->store_line(csv_line);
		}
	}

	f->close();
	return OK;
}

// Import devices from CSV. Hard-rejects a ScanGroup column (match driver-engine).
Error industrial_import_devices_csv(Ref<IndustrialProject> p_project, const String &p_path) {
	if (p_project.is_null()) {
		return ERR_INVALID_PARAMETER;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return ERR_CANT_OPEN;
	}

	const String header = f->get_line();
	const Error header_err = reject_scan_group_csv_header(header);
	if (header_err != OK) {
		f->close();
		return header_err;
	}

	while (!f->eof_reached()) {
		String line = f->get_line().strip_edges();
		if (line.is_empty()) {
			continue;
		}

		// Simple CSV parsing (no embedded quotes for now).
		Vector<String> fields = line.split(",");
		if (fields.size() < 4) {
			continue;
		}

		IndustrialDeviceData dev;
		dev.name = fields[0].strip_edges().unquote();
		dev.description = fields[1].strip_edges().unquote();

		String driver_name = fields[2].strip_edges();
		StringList driver_names = industrial_get_driver_names();
		for (int i = 0; i < driver_names.size(); i++) {
			if (driver_names[i] == driver_name) {
				dev.driver = i;
				break;
			}
		}

		dev.enabled = fields[3].strip_edges() == "true";

		if (fields.size() >= 5) {
			String params_json = fields[4].strip_edges().unquote();
			if (!params_json.is_empty()) {
				Variant parsed = JSON::parse_string(params_json);
				if (parsed.get_type() == Variant::DICTIONARY) {
					dev.connection_params = parsed;
				}
			}
		}

		p_project->add_device(dev);
	}

	f->close();
	return OK;
}

// Import tags from CSV. Hard-rejects a ScanGroup column (match driver-engine).
Error industrial_import_tags_csv(Ref<IndustrialProject> p_project, const String &p_path) {
	if (p_project.is_null()) {
		return ERR_INVALID_PARAMETER;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return ERR_CANT_OPEN;
	}

	const String header = f->get_line();
	const Error header_err = reject_scan_group_csv_header(header);
	if (header_err != OK) {
		f->close();
		return header_err;
	}

	while (!f->eof_reached()) {
		String line = f->get_line().strip_edges();
		if (line.is_empty()) {
			continue;
		}

		Vector<String> fields = line.split(",");
		if (fields.size() < 4) {
			continue;
		}

		String device_name = fields[0].strip_edges().unquote();
		int dev_idx = p_project->find_device_index(device_name);
		if (dev_idx < 0) {
			continue;
		}

		IndustrialTagData tag;
		tag.address = fields[1].strip_edges().unquote();
		tag.name = fields[2].strip_edges().unquote();

		String type_name = fields[3].strip_edges();
		StringList type_names = industrial_get_data_type_names();
		for (int i = 0; i < type_names.size(); i++) {
			if (type_names[i] == type_name) {
				tag.data_type = i;
				break;
			}
		}

		if (fields.size() >= 5) {
			tag.writable = fields[4].strip_edges() == "true";
		}
		if (fields.size() >= 6) {
			tag.scale = fields[5].strip_edges().to_float();
		}
		if (fields.size() >= 7) {
			tag.unit = fields[6].strip_edges().unquote();
		}

		p_project->add_tag(dev_idx, tag);
	}

	f->close();
	return OK;
}
