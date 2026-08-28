#include "industrial_csv_io.h"
#include "industrial_project.h"
#include "industrial_driver_schema.h"

#include "core/io/file_access.h"
#include "core/string/ustring.h"
#include "core/io/json.h"
#include "core/templates/vector.h"

// Export devices to CSV (EBPro 7-column compatible format).
// Columns: Name, Description, Driver, ScanGroup, Enabled, ConnectionParams(JSON), TagCount
Error industrial_export_devices_csv(Ref<IndustrialProject> p_project, const String &p_path) {
	if (p_project.is_null()) {
		return ERR_INVALID_PARAMETER;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		return ERR_FILE_CANT_WRITE;
	}

	// Header.
	f->store_line("Name,Description,Driver,ScanGroup,Enabled,ConnectionParams,TagCount");

	int count = p_project->get_device_count();
	for (int i = 0; i < count; i++) {
		const auto &dev = p_project->get_device(i);
		String csv_line;
		csv_line += "\"" + dev.name.replace("\"", "\"\"") + "\",";
		csv_line += "\"" + dev.description.replace("\"", "\"\"") + "\",";
		csv_line += industrial_get_driver_name(dev.driver) + ",";
		csv_line += "\"" + dev.scan_group.replace("\"", "\"\"") + "\",";
		csv_line += dev.enabled ? "true" : "false";
		csv_line += ",";
		// Serialize connection params as JSON string.
		String params_json = JSON::stringify(dev.connection_params);
		csv_line += "\"" + params_json.replace("\"", "\"\"") + "\",";
		csv_line += itos((int)dev.tags.size());
		f->store_line(csv_line);
	}

	f->close();
	return OK;
}

// Export tags to CSV (EBPro-compatible format).
// Columns: Device, Address, Name, DataType, ScanGroup, Writable, Scale, Unit
Error industrial_export_tags_csv(Ref<IndustrialProject> p_project, const String &p_path) {
	if (p_project.is_null()) {
		return ERR_INVALID_PARAMETER;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		return ERR_FILE_CANT_WRITE;
	}

	f->store_line("Device,Address,Name,DataType,ScanGroup,Writable,Scale,Unit");

	int dev_count = p_project->get_device_count();
	for (int di = 0; di < dev_count; di++) {
		const auto &dev = p_project->get_device(di);
		for (const auto &tag : dev.tags) {
			String csv_line;
			csv_line += "\"" + dev.name.replace("\"", "\"\"") + "\",";
			csv_line += "\"" + tag.address.replace("\"", "\"\"") + "\",";
			csv_line += "\"" + tag.name.replace("\"", "\"\"") + "\",";
			csv_line += industrial_get_data_type_name(tag.data_type) + ",";
			csv_line += "\"" + tag.scan_group.replace("\"", "\"\"") + "\",";
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

// Import devices from CSV.
Error industrial_import_devices_csv(Ref<IndustrialProject> p_project, const String &p_path) {
	if (p_project.is_null()) {
		return ERR_INVALID_PARAMETER;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return ERR_CANT_OPEN;
	}

	// Skip header.
	f->get_line();

	while (!f->eof_reached()) {
		String line = f->get_line().strip_edges();
		if (line.is_empty()) continue;

		// Simple CSV parsing (no embedded quotes for now).
		Vector<String> fields = line.split(",");
		if (fields.size() < 5) continue;

		IndustrialDeviceData dev;
		dev.name = fields[0].strip_edges().unquote();
		dev.description = fields[1].strip_edges().unquote();

		// Find driver by name.
		String driver_name = fields[2].strip_edges();
		StringList driver_names = industrial_get_driver_names();
		for (int i = 0; i < driver_names.size(); i++) {
			if (driver_names[i] == driver_name) {
				dev.driver = i;
				break;
			}
		}

		dev.scan_group = fields[3].strip_edges().unquote();
		dev.enabled = fields[4].strip_edges() == "true";

		if (fields.size() >= 6) {
			String params_json = fields[5].strip_edges().unquote();
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

// Import tags from CSV.
Error industrial_import_tags_csv(Ref<IndustrialProject> p_project, const String &p_path) {
	if (p_project.is_null()) {
		return ERR_INVALID_PARAMETER;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return ERR_CANT_OPEN;
	}

	// Skip header.
	f->get_line();

	while (!f->eof_reached()) {
		String line = f->get_line().strip_edges();
		if (line.is_empty()) continue;

		Vector<String> fields = line.split(",");
		if (fields.size() < 4) continue;

		String device_name = fields[0].strip_edges().unquote();
		int dev_idx = p_project->find_device_index(device_name);
		if (dev_idx < 0) continue;

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

		if (fields.size() >= 5) tag.scan_group = fields[4].strip_edges().unquote();
		if (fields.size() >= 6) tag.writable = fields[5].strip_edges() == "true";
		if (fields.size() >= 7) tag.scale = fields[6].strip_edges().to_float();
		if (fields.size() >= 8) tag.unit = fields[7].strip_edges().unquote();

		p_project->add_tag(dev_idx, tag);
	}

	f->close();
	return OK;
}
