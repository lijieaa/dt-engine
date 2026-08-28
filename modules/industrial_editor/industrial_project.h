#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "scene/gui/tree.h"

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
	String description;
	String schema; // "absolute" | "symbolic"
	String address_mode; // "bit" | "word"
	String address_type;
	String data_format; // catalog id (i16, f32, bit, …)
	String address; // catalog offset for absolute; legacy raw address when schema unset
	int db_number = 0;
	int length = 0;
	String symbol;
	int data_type = TYPE_BOOL; // legacy UI enum; compat read only on wire
	String scan_group;
	bool writable = false;
	int poll_interval = 0;
	double scale = 1.0; // legacy scalar scale
	Dictionary scale_obj; // {enabled, raw_min, raw_max, eng_min, eng_max}
	String unit;
};

// A single device definition (replaces EBPro 0x24294-byte device struct).
struct IndustrialDeviceData {
	String name;
	String description;
	int driver = 0; // Index into driver catalog; -1 when driver_key is unknown
	String driver_key; // Wire-format catalog key (authoritative when driver == -1)
	String dev_type = "device";
	String location_mode = "Local";
	String remote_hmi_ip;
	String interface_type;
	String ip;
	int port = 0;
	bool use_udp = false;
	String serial_port;
	String baud_rate;
	int data_bits = 0;
	String parity;
	int stop_bits = 0;
	String flow_control;
	int station_no = 0;
	int broadcast_station_no = 0;
	bool use_station_variable = false;
	int timeout = 0;
	int comm_delay = 0;
	int retries = 0;
	int max_read_words = 0;
	int max_write_words = 0;
	int poll_interval = 0;
	int block_size_words = 0;
	Dictionary options;
	String scan_group;
	bool enabled = true;
	Dictionary connection_params; // deprecated: UI bridge; flat+options is authoritative
	Vector<IndustrialTagData> tags;
};

// EBPro §3.1 device dialog field groups.
enum IndustrialDeviceFieldGroup {
	IND_DEVICE_GROUP_COMMON = 0,
	IND_DEVICE_GROUP_INTERFACE = 1,
	IND_DEVICE_GROUP_TUNING = 2,
	IND_DEVICE_GROUP_PROTOCOL = 3,
};

IndustrialDeviceFieldGroup industrial_classify_device_field(const String &p_key);
bool industrial_is_known_device_flat_key(const String &p_key);
Dictionary industrial_device_param_dict(const IndustrialDeviceData &p_dev);
void industrial_apply_param_dict_to_device(IndustrialDeviceData &p_dev, const Dictionary &p_params);
void industrial_sync_device_connection_params(IndustrialDeviceData &p_dev);

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
	Vector<IndustrialDeviceData> devices;
	Vector<IndustrialScanGroup> scan_groups;

	static IndustrialTagData s_empty_tag;
	static IndustrialDeviceData s_empty_device;
};
