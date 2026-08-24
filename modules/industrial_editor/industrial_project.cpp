#include "industrial_project.h"
#include "industrial_driver_schema.h"

#include "core/io/file_access.h"
#include "core/variant/variant.h"
#include "core/io/json.h"

#include <algorithm>

IndustrialTagData IndustrialProject::s_empty_tag;
IndustrialDeviceData IndustrialProject::s_empty_device;

IndustrialProject::IndustrialProject() {}

IndustrialProject::~IndustrialProject() {}

int IndustrialProject::get_device_count() const {
	return (int)devices.size();
}

const IndustrialDeviceData &IndustrialProject::get_device(int p_index) const {
	if (p_index < 0 || p_index >= (int)devices.size()) {
		return s_empty_device;
	}
	return devices[p_index];
}

int IndustrialProject::find_device_index(const String &p_name) const {
	for (int i = 0; i < (int)devices.size(); i++) {
		if (devices[i].name == p_name) {
			return i;
		}
	}
	return -1;
}

bool IndustrialProject::add_device(const IndustrialDeviceData &p_device) {
	if (p_device.name.is_empty()) {
		return false;
	}
	if (find_device_index(p_device.name) >= 0) {
		return false; // Duplicate name.
	}
	devices.push_back(p_device);
	return true;
}

bool IndustrialProject::update_device(int p_index, const IndustrialDeviceData &p_device) {
	if (p_index < 0 || p_index >= (int)devices.size()) {
		return false;
	}
	if (p_device.name.is_empty()) {
		return false;
	}
	devices[p_index] = p_device;
	return true;
}

bool IndustrialProject::remove_device(int p_index) {
	if (p_index < 0 || p_index >= (int)devices.size()) {
		return false;
	}
	devices.erase(devices.begin() + p_index);
	return true;
}

bool IndustrialProject::duplicate_device(int p_index) {
	if (p_index < 0 || p_index >= (int)devices.size()) {
		return false;
	}
	IndustrialDeviceData copy = devices[p_index];
	copy.name += " (Copy)";
	int suffix = 1;
	while (find_device_index(copy.name) >= 0) {
		copy.name = devices[p_index].name + " (Copy " + itos(++suffix) + ")";
	}
	devices.push_back(copy);
	return true;
}

int IndustrialProject::get_tag_count() const {
	int count = 0;
	for (const auto &dev : devices) {
		count += (int)dev.tags.size();
	}
	return count;
}

const IndustrialTagData &IndustrialProject::get_tag(int p_index) const {
	// Linear scan across all devices.
	int idx = 0;
	for (const auto &dev : devices) {
		if (p_index < idx + (int)dev.tags.size()) {
			return dev.tags[p_index - idx];
		}
		idx += (int)dev.tags.size();
	}
	return s_empty_tag;
}

int IndustrialProject::find_tag_index(const String &p_device_name, const String &p_tag_name) const {
	int dev_idx = find_device_index(p_device_name);
	if (dev_idx < 0) {
		return -1;
	}
	const auto &tags = devices[dev_idx].tags;
	for (int i = 0; i < (int)tags.size(); i++) {
		if (tags[i].name == p_tag_name) {
			return i;
		}
	}
	return -1;
}

bool IndustrialProject::add_tag(int p_device_index, const IndustrialTagData &p_tag) {
	if (p_device_index < 0 || p_device_index >= (int)devices.size()) {
		return false;
	}
	if (p_tag.name.is_empty()) {
		return false;
	}
	// Check for duplicate tag name within the device.
	for (const auto &tag : devices[p_device_index].tags) {
		if (tag.name == p_tag.name) {
			return false;
		}
	}
	devices[p_device_index].tags.push_back(p_tag);
	return true;
}

bool IndustrialProject::update_tag(int p_device_index, int p_tag_index, const IndustrialTagData &p_tag) {
	if (p_device_index < 0 || p_device_index >= (int)devices.size()) {
		return false;
	}
	auto &tags = devices[p_device_index].tags;
	if (p_tag_index < 0 || p_tag_index >= (int)tags.size()) {
		return false;
	}
	if (p_tag.name.is_empty()) {
		return false;
	}
	tags[p_tag_index] = p_tag;
	return true;
}

bool IndustrialProject::remove_tag(int p_device_index, int p_tag_index) {
	if (p_device_index < 0 || p_device_index >= (int)devices.size()) {
		return false;
	}
	auto &tags = devices[p_device_index].tags;
	if (p_tag_index < 0 || p_tag_index >= (int)tags.size()) {
		return false;
	}
	tags.erase(tags.begin() + p_tag_index);
	return true;
}

int IndustrialProject::get_tag_count_for_device(int p_device_index) const {
	if (p_device_index < 0 || p_device_index >= (int)devices.size()) {
		return 0;
	}
	return (int)devices[p_device_index].tags.size();
}

int IndustrialProject::get_scan_group_count() const {
	return (int)scan_groups.size();
}

const IndustrialScanGroup &IndustrialProject::get_scan_group(int p_index) const {
	if (p_index < 0 || p_index >= (int)scan_groups.size()) {
		static IndustrialScanGroup s_empty;
		return s_empty;
	}
	return scan_groups[p_index];
}

bool IndustrialProject::add_scan_group(const IndustrialScanGroup &p_group) {
	if (p_group.name.is_empty()) {
		return false;
	}
	for (const auto &g : scan_groups) {
		if (g.name == p_group.name) {
			return false;
		}
	}
	scan_groups.push_back(p_group);
	return true;
}

bool IndustrialProject::update_scan_group(int p_index, const IndustrialScanGroup &p_group) {
	if (p_index < 0 || p_index >= (int)scan_groups.size()) {
		return false;
	}
	if (p_group.name.is_empty()) {
		return false;
	}
	scan_groups[p_index] = p_group;
	return true;
}

bool IndustrialProject::remove_scan_group(int p_index) {
	if (p_index < 0 || p_index >= (int)scan_groups.size()) {
		return false;
	}
	scan_groups.erase(scan_groups.begin() + p_index);
	return true;
}

Dictionary IndustrialProject::to_dict() const {
	Dictionary result;

	// Scan groups.
	Array groups_arr;
	for (const auto &g : scan_groups) {
		Dictionary gd;
		gd["name"] = g.name;
		gd["interval_ms"] = g.interval_ms;
		groups_arr.append(gd);
	}
	result["scan_groups"] = groups_arr;

	// Devices.
	Array devices_arr;
	for (const auto &dev : devices) {
		Dictionary dd;
		dd["name"] = dev.name;
		dd["description"] = dev.description;
		dd["driver"] = dev.driver;
		dd["scan_group"] = dev.scan_group;
		dd["enabled"] = dev.enabled;
		dd["connection_params"] = dev.connection_params;

		Array tags_arr;
		for (const auto &tag : dev.tags) {
			Dictionary td;
			td["name"] = tag.name;
			td["address"] = tag.address;
			td["data_type"] = tag.data_type;
			td["scan_group"] = tag.scan_group;
			td["writable"] = tag.writable;
			td["scale"] = tag.scale;
			td["unit"] = tag.unit;
			tags_arr.append(td);
		}
		dd["tags"] = tags_arr;
		devices_arr.append(dd);
	}
	result["devices"] = devices_arr;

	return result;
}

void IndustrialProject::from_dict(const Dictionary &p_data) {
	devices.clear();
	scan_groups.clear();

	// Scan groups.
	Array groups_arr = p_data.get("scan_groups", Array());
	for (int i = 0; i < groups_arr.size(); i++) {
		Dictionary gd = groups_arr[i];
		IndustrialScanGroup g;
		g.name = gd.get("name", "");
		g.interval_ms = gd.get("interval_ms", 500);
		if (!g.name.is_empty()) {
			scan_groups.push_back(g);
		}
	}

	// Devices.
	Array devices_arr = p_data.get("devices", Array());
	for (int i = 0; i < devices_arr.size(); i++) {
		Dictionary dd = devices_arr[i];
		IndustrialDeviceData dev;
		dev.name = dd.get("name", "");
		dev.description = dd.get("description", "");
		dev.driver = dd.get("driver", 0);
		dev.scan_group = dd.get("scan_group", "");
		dev.enabled = dd.get("enabled", true);
		dev.connection_params = dd.get("connection_params", Dictionary());

		Array tags_arr = dd.get("tags", Array());
		for (int j = 0; j < tags_arr.size(); j++) {
			Dictionary td = tags_arr[j];
			IndustrialTagData tag;
			tag.name = td.get("name", "");
			tag.address = td.get("address", "");
			tag.data_type = td.get("data_type", 0);
			tag.scan_group = td.get("scan_group", "");
			tag.writable = td.get("writable", false);
			tag.scale = td.get("scale", 1.0);
			tag.unit = td.get("unit", "");
			if (!tag.name.is_empty()) {
				dev.tags.push_back(tag);
			}
		}
		if (!dev.name.is_empty()) {
			devices.push_back(dev);
		}
	}
}

Error IndustrialProject::save_to_file(const String &p_path) {
	Dictionary data = to_dict();
	String json = JSON::stringify(data, "\t");
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		return ERR_FILE_CANT_WRITE;
	}
	f->store_string(json);
	f->close();
	return OK;
}

Error IndustrialProject::load_from_file(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return ERR_CANT_OPEN;
	}
	String json = f->get_as_text();
	f->close();

	// Godot 4 JSON::parse_string 返回 Variant(失败时返回 NIL);
	// 旧版的 parse_string(s, &out, &err_str, &err_line) 已被替换为 _parse_string 私有 API。
	Variant parsed = JSON::parse_string(json);
	if (parsed.get_type() != Variant::DICTIONARY) {
		return ERR_INVALID_DATA;
	}
	from_dict(parsed);
	return OK;
}

Array IndustrialProject::validate() const {
	Array errors;

	// Check duplicate scan group names.
	for (int i = 0; i < (int)scan_groups.size(); i++) {
		for (int j = i + 1; j < (int)scan_groups.size(); j++) {
			if (scan_groups[i].name == scan_groups[j].name) {
				errors.append(vformat(TTR("Duplicate scan group name: '%s'."), scan_groups[i].name));
			}
		}
	}

	// Check devices.
	for (int i = 0; i < (int)devices.size(); i++) {
		const auto &dev = devices[i];
		if (dev.name.is_empty()) {
			errors.append(TTR("Device has empty name."));
			continue;
		}
		if (dev.driver < 0 || dev.driver >= industrial_get_driver_count()) {
			errors.append(vformat(TTR("Device '%s' has invalid driver."), dev.name));
		}
		if (!dev.scan_group.is_empty()) {
			bool found = false;
			for (const auto &g : scan_groups) {
				if (g.name == dev.scan_group) {
					found = true;
					break;
				}
			}
			if (!found) {
				errors.append(vformat(TTR("Device '%s' references missing scan group '%s'."), dev.name, dev.scan_group));
			}
		}

		// Check tags within device.
		for (int j = 0; j < (int)dev.tags.size(); j++) {
			const auto &tag = dev.tags[j];
			if (tag.name.is_empty()) {
				errors.append(vformat(TTR("Device '%s' has tag with empty name."), dev.name));
			}
			if (tag.data_type < 0 || tag.data_type >= TYPE_MAX) {
				errors.append(vformat(TTR("Tag '%s' on device '%s' has invalid data type."), tag.name, dev.name));
			}
		}

		// Check duplicate tag names within device.
		for (int j = 0; j < (int)dev.tags.size(); j++) {
			for (int k = j + 1; k < (int)dev.tags.size(); k++) {
				if (dev.tags[j].name == dev.tags[k].name) {
					errors.append(vformat(TTR("Duplicate tag name '%s' in device '%s'."), dev.tags[j].name, dev.name));
				}
			}
		}
	}

	// Check duplicate device names.
	for (int i = 0; i < (int)devices.size(); i++) {
		for (int j = i + 1; j < (int)devices.size(); j++) {
			if (devices[i].name == devices[j].name) {
				errors.append(vformat(TTR("Duplicate device name: '%s'."), devices[i].name));
			}
		}
	}

	return errors;
}

void IndustrialProject::notify_changed() {
	// This is a hook for future signal emission from EditorNode.
	// For now, UI docks poll the model directly.
}

// Placeholder CSV implementations (full implementation in industrial_csv_io.cpp).
Error IndustrialProject::export_devices_csv(const String &p_path) const {
	return ERR_UNCONFIGURED;
}
Error IndustrialProject::export_tags_csv(const String &p_path) const {
	return ERR_UNCONFIGURED;
}
Error IndustrialProject::import_devices_csv(const String &p_path) {
	return ERR_UNCONFIGURED;
}
Error IndustrialProject::import_tags_csv(const String &p_path) {
	return ERR_UNCONFIGURED;
}
