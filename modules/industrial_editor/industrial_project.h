#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "scene/gui/tree.h"

#include <vector>

// Forward declarations.
class IndustrialDeviceDock;
class IndustrialTagDock;

// Data types supported by driver tags (aligns with EBPro §18.10.13 strategy classes).
enum IndustrialDataType {
	TYPE_BOOL = 0,
	TYPE_INT16,
	TYPE_INT32,
	TYPE_UINT16,
	TYPE_UINT32,
	TYPE_BYTE,
	TYPE_FLOAT,
	TYPE_DOUBLE,
	TYPE_BCD16,
	TYPE_BCD32,
	TYPE_REAL32,
	TYPE_REAL64,
	TYPE_MAX
};

// DEPRECATED: The IndustrialDriver enum is obsolete. Driver indices are now
// dynamic (0-80) and come from the 81-entry DriverMeta catalog in
// industrial_driver_schema.cpp/.h. Use industrial_get_driver_count() to get
// the current count and industrial_get_driver_name()/meta() for metadata.
// The old 7-entry enum is retained below (commented out) for historical
// reference only.
/*
enum IndustrialDriver {
	DRIVER_SIEMENS_S7 = 0,
	DRIVER_MODBUS_RTU,
	DRIVER_MODBUS_TCP,
	DRIVER_BACNET_MSTP,
	DRIVER_SECS_GEM,
	DRIVER_MITSUBISHI_FX,
	DRIVER_OMRON_FINS,
	DRIVER_MAX
};
*/

// A single tag definition (replaces EBPro 0x1064-byte address item slot).
struct IndustrialTagData {
	String name;
	String address;
	int data_type = TYPE_BOOL;
	String scan_group;
	bool writable = false;
	double scale = 1.0;
	String unit;
};

// A single device definition (replaces EBPro 0x24294-byte device struct).
struct IndustrialDeviceData {
	String name;
	String description;
	int driver = 0; // Index into the 81-entry driver catalog (0 = Siemens S7-1200/1500)
	String scan_group;
	bool enabled = true;
	Dictionary connection_params; // Protocol-specific fields (IP, port, baud, etc.)
	std::vector<IndustrialTagData> tags;
};

// Scan group definition.
struct IndustrialScanGroup {
	String name;
	int interval_ms = 500;
};

// IndustrialProject is the in-memory project model for device/tag management.
// It owns the device list, tag list, and scan groups. All UI docks and dialogs
// interact through this model.
class IndustrialProject : public RefCounted {
	GDCLASS(IndustrialProject, RefCounted);

public:
	IndustrialProject();
	~IndustrialProject() override;

	// --- Device operations ---
	int get_device_count() const;
	const IndustrialDeviceData &get_device(int p_index) const;
	int find_device_index(const String &p_name) const;

	bool add_device(const IndustrialDeviceData &p_device);
	bool update_device(int p_index, const IndustrialDeviceData &p_device);
	bool remove_device(int p_index);
	bool duplicate_device(int p_index);

	// --- Tag operations ---
	int get_tag_count() const;
	const IndustrialTagData &get_tag(int p_index) const;
	int find_tag_index(const String &p_device_name, const String &p_tag_name) const;

	bool add_tag(int p_device_index, const IndustrialTagData &p_tag);
	bool update_tag(int p_device_index, int p_tag_index, const IndustrialTagData &p_tag);
	bool remove_tag(int p_device_index, int p_tag_index);

	int get_tag_count_for_device(int p_device_index) const;

	// --- Scan group operations ---
	int get_scan_group_count() const;
	const IndustrialScanGroup &get_scan_group(int p_index) const;

	bool add_scan_group(const IndustrialScanGroup &p_group);
	bool update_scan_group(int p_index, const IndustrialScanGroup &p_group);
	bool remove_scan_group(int p_index);

	// --- Persistence ---
	Error save_to_file(const String &p_path);
	Error load_from_file(const String &p_path);
	Dictionary to_dict() const;
	void from_dict(const Dictionary &p_data);

	// --- Validation ---
	Array validate() const; // Returns array of error messages.

	// --- CSV ---
	Error export_devices_csv(const String &p_path) const;
	Error export_tags_csv(const String &p_path) const;
	Error import_devices_csv(const String &p_path);
	Error import_tags_csv(const String &p_path);

	// --- Signals (emitted via EditorNode) ---
	void notify_changed();

private:
	std::vector<IndustrialDeviceData> devices;
	std::vector<IndustrialScanGroup> scan_groups;

	static IndustrialTagData s_empty_tag;
	static IndustrialDeviceData s_empty_device;
};
