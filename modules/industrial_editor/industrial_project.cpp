#include "industrial_project.h"
#include "industrial_driver_schema.h"

#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/variant/variant.h"

namespace {

int dict_get_int(const Dictionary &p_dict, const String &p_key, int p_default = 0) {
	Variant v = p_dict.get(p_key, p_default);
	if (v.get_type() == Variant::INT || v.get_type() == Variant::FLOAT) {
		return int(v);
	}
	if (v.get_type() == Variant::STRING) {
		return String(v).to_int();
	}
	return p_default;
}

bool dict_get_bool(const Dictionary &p_dict, const String &p_key, bool p_default = false) {
	Variant v = p_dict.get(p_key, p_default);
	if (v.get_type() == Variant::BOOL) {
		return v;
	}
	if (v.get_type() == Variant::STRING) {
		const String s = v;
		return s == "true" || s == "1";
	}
	return p_default;
}

String dict_get_string(const Dictionary &p_dict, const String &p_key, const String &p_default = String()) {
	Variant v = p_dict.get(p_key, p_default);
	if (v.get_type() == Variant::NIL) {
		return p_default;
	}
	return String(v);
}

String data_format_from_legacy_type(int p_type) {
	switch (p_type) {
		case TYPE_BOOL:
			return "bit";
		case TYPE_INT16:
			return "i16";
		case TYPE_INT32:
			return "i32";
		case TYPE_UINT16:
			return "u16";
		case TYPE_UINT32:
			return "u32";
		case TYPE_BYTE:
			return "u8";
		case TYPE_FLOAT:
		case TYPE_REAL32:
			return "f32";
		case TYPE_DOUBLE:
		case TYPE_REAL64:
			return "f64";
		case TYPE_BCD16:
			return "bcd16";
		case TYPE_BCD32:
			return "bcd32";
		default:
			return String();
	}
}

int legacy_type_from_data_format(const String &p_format) {
	if (p_format == "bit" || p_format == "bool") {
		return TYPE_BOOL;
	}
	if (p_format == "i16" || p_format == "int16") {
		return TYPE_INT16;
	}
	if (p_format == "i32" || p_format == "int32") {
		return TYPE_INT32;
	}
	if (p_format == "u16" || p_format == "uint16") {
		return TYPE_UINT16;
	}
	if (p_format == "u32" || p_format == "uint32") {
		return TYPE_UINT32;
	}
	if (p_format == "u8" || p_format == "byte") {
		return TYPE_BYTE;
	}
	if (p_format == "f32" || p_format == "float" || p_format == "real32") {
		return TYPE_FLOAT;
	}
	if (p_format == "f64" || p_format == "double" || p_format == "real64") {
		return TYPE_DOUBLE;
	}
	if (p_format == "bcd16") {
		return TYPE_BCD16;
	}
	if (p_format == "bcd32") {
		return TYPE_BCD32;
	}
	return -1;
}

bool apply_known_device_key(IndustrialDeviceData &p_dev, const String &p_key, const Variant &p_value) {
	if (p_key == "ip") {
		if (p_dev.ip.is_empty()) {
			p_dev.ip = String(p_value);
		}
		return true;
	}
	if (p_key == "port") {
		if (p_dev.port == 0) {
			p_dev.port = int(p_value);
		}
		return true;
	}
	if (p_key == "endpoint") {
		if (p_dev.ip.is_empty()) {
			const String ep = String(p_value);
			const int colon = ep.rfind(":");
			if (colon > 0) {
				p_dev.ip = ep.substr(0, colon);
				p_dev.port = ep.substr(colon + 1).to_int();
			} else if (!ep.is_empty()) {
				p_dev.ip = ep;
			}
		}
		return true;
	}
	if (p_key == "use_udp") {
		p_dev.use_udp = bool(p_value);
		return true;
	}
	if (p_key == "serial_port") {
		if (p_dev.serial_port.is_empty()) {
			p_dev.serial_port = String(p_value);
		}
		return true;
	}
	if (p_key == "baud_rate") {
		if (p_dev.baud_rate.is_empty()) {
			p_dev.baud_rate = String(p_value);
		}
		return true;
	}
	if (p_key == "data_bits") {
		if (p_dev.data_bits == 0) {
			p_dev.data_bits = int(p_value);
		}
		return true;
	}
	if (p_key == "parity") {
		if (p_dev.parity.is_empty()) {
			p_dev.parity = String(p_value);
		}
		return true;
	}
	if (p_key == "stop_bits") {
		if (p_dev.stop_bits == 0) {
			p_dev.stop_bits = int(p_value);
		}
		return true;
	}
	if (p_key == "flow_control") {
		if (p_dev.flow_control.is_empty()) {
			p_dev.flow_control = String(p_value);
		}
		return true;
	}
	if (p_key == "station_no") {
		if (p_dev.station_no == 0) {
			p_dev.station_no = int(p_value);
		}
		return true;
	}
	if (p_key == "broadcast_station_no") {
		if (p_dev.broadcast_station_no == 0) {
			p_dev.broadcast_station_no = int(p_value);
		}
		return true;
	}
	if (p_key == "use_station_variable") {
		p_dev.use_station_variable = bool(p_value);
		return true;
	}
	if (p_key == "timeout") {
		if (p_dev.timeout == 0) {
			p_dev.timeout = int(p_value);
		}
		return true;
	}
	if (p_key == "comm_delay") {
		if (p_dev.comm_delay == 0) {
			p_dev.comm_delay = int(p_value);
		}
		return true;
	}
	if (p_key == "retries") {
		if (p_dev.retries == 0) {
			p_dev.retries = int(p_value);
		}
		return true;
	}
	if (p_key == "max_read_words") {
		if (p_dev.max_read_words == 0) {
			p_dev.max_read_words = int(p_value);
		}
		return true;
	}
	if (p_key == "max_write_words") {
		if (p_dev.max_write_words == 0) {
			p_dev.max_write_words = int(p_value);
		}
		return true;
	}
	if (p_key == "poll_interval") {
		if (p_dev.poll_interval == 0) {
			p_dev.poll_interval = int(p_value);
		}
		return true;
	}
	if (p_key == "block_size_words") {
		if (p_dev.block_size_words == 0) {
			p_dev.block_size_words = int(p_value);
		}
		return true;
	}
	if (p_key == "location_mode") {
		if (p_dev.location_mode == "Local") {
			p_dev.location_mode = String(p_value);
		}
		return true;
	}
	if (p_key == "remote_hmi_ip") {
		if (p_dev.remote_hmi_ip.is_empty()) {
			p_dev.remote_hmi_ip = String(p_value);
		}
		return true;
	}
	if (p_key == "dev_type") {
		if (p_dev.dev_type == "device") {
			p_dev.dev_type = String(p_value);
		}
		return true;
	}
	if (p_key == "interface_type") {
		if (p_dev.interface_type.is_empty()) {
			p_dev.interface_type = String(p_value);
		}
		return true;
	}
	return false;
}

void lift_connection_params(IndustrialDeviceData &p_dev) {
	if (p_dev.connection_params.is_empty()) {
		return;
	}
	Array keys = p_dev.connection_params.keys();
	for (int i = 0; i < keys.size(); i++) {
		const String key = keys[i];
		const Variant value = p_dev.connection_params[key];
		if (apply_known_device_key(p_dev, key, value)) {
			continue;
		}
		if (!p_dev.options.has(key)) {
			p_dev.options[key] = value;
		}
	}
	p_dev.connection_params.clear();
}

void sync_connection_params_from_flat(IndustrialDeviceData &p_dev) {
	p_dev.connection_params.clear();
	auto set_str = [&](const String &p_key, const String &p_value) {
		if (!p_value.is_empty()) {
			p_dev.connection_params[p_key] = p_value;
		}
	};
	auto set_int = [&](const String &p_key, int p_value) {
		if (p_value != 0) {
			p_dev.connection_params[p_key] = p_value;
		}
	};
	auto set_bool = [&](const String &p_key, bool p_value) {
		if (p_value) {
			p_dev.connection_params[p_key] = p_value;
		}
	};

	set_str("dev_type", p_dev.dev_type == "device" ? String() : p_dev.dev_type);
	set_str("location_mode", p_dev.location_mode == "Local" ? String() : p_dev.location_mode);
	set_str("remote_hmi_ip", p_dev.remote_hmi_ip);
	set_str("interface_type", p_dev.interface_type);
	set_str("ip", p_dev.ip);
	set_int("port", p_dev.port);
	set_bool("use_udp", p_dev.use_udp);
	set_str("serial_port", p_dev.serial_port);
	set_str("baud_rate", p_dev.baud_rate);
	set_int("data_bits", p_dev.data_bits);
	set_str("parity", p_dev.parity);
	set_int("stop_bits", p_dev.stop_bits);
	set_str("flow_control", p_dev.flow_control);
	set_int("station_no", p_dev.station_no);
	set_int("broadcast_station_no", p_dev.broadcast_station_no);
	set_bool("use_station_variable", p_dev.use_station_variable);
	set_int("timeout", p_dev.timeout);
	set_int("comm_delay", p_dev.comm_delay);
	set_int("retries", p_dev.retries);
	set_int("max_read_words", p_dev.max_read_words);
	set_int("max_write_words", p_dev.max_write_words);
	set_int("poll_interval", p_dev.poll_interval);
	set_int("block_size_words", p_dev.block_size_words);

	Array opt_keys = p_dev.options.keys();
	for (int i = 0; i < opt_keys.size(); i++) {
		const String key = opt_keys[i];
		p_dev.connection_params[key] = p_dev.options[key];
	}
}

void ensure_device_flat_for_wire(IndustrialDeviceData &p_dev) {
	lift_connection_params(p_dev);
}

void parse_driver_from_variant(const Variant &p_value, int &r_driver, String &r_driver_key) {
	r_driver_key = String();
	switch (p_value.get_type()) {
		case Variant::INT:
		case Variant::FLOAT: {
			r_driver = int(p_value);
			if (r_driver >= 0 && r_driver < industrial_get_driver_count()) {
				r_driver_key = industrial_get_driver_key(r_driver);
			}
		} break;
		case Variant::STRING: {
			r_driver_key = String(p_value);
			if (r_driver_key.is_empty()) {
				r_driver = -1;
				break;
			}
			const int idx = industrial_find_driver_index_by_key(r_driver_key);
			r_driver = idx >= 0 ? idx : -1;
		} break;
		default:
			r_driver = 0;
			r_driver_key = industrial_get_driver_key(0);
			break;
	}
}

String infer_tag_schema(const IndustrialTagData &p_tag) {
	if (!p_tag.schema.is_empty()) {
		return p_tag.schema;
	}
	if (!p_tag.symbol.is_empty() && p_tag.address_type.is_empty()) {
		return "symbolic";
	}
	if (!p_tag.address_type.is_empty()) {
		return "absolute";
	}
	return String();
}

String tag_wire_data_format(const IndustrialTagData &p_tag) {
	if (!p_tag.data_format.is_empty()) {
		return p_tag.data_format;
	}
	return data_format_from_legacy_type(p_tag.data_type);
}

void put_if_non_empty(Dictionary &p_dict, const String &p_key, const String &p_value) {
	if (!p_value.is_empty()) {
		p_dict[p_key] = p_value;
	}
}

void put_if_nonzero(Dictionary &p_dict, const String &p_key, int p_value) {
	if (p_value != 0) {
		p_dict[p_key] = p_value;
	}
}

Dictionary device_to_dict(const IndustrialDeviceData &p_dev) {
	IndustrialDeviceData dev = p_dev;
	ensure_device_flat_for_wire(dev);

	Dictionary dd;
	dd["id"] = dev.name;
	dd["name"] = dev.name;
	if (!dev.description.is_empty()) {
		dd["description"] = dev.description;
	}
	const String driver_wire = !dev.driver_key.is_empty()
			? dev.driver_key
			: industrial_get_driver_key(dev.driver);
	if (!driver_wire.is_empty()) {
		dd["driver"] = driver_wire;
	}
	dd["enabled"] = dev.enabled;
	put_if_non_empty(dd, "scan_group", dev.scan_group);

	put_if_non_empty(dd, "dev_type", dev.dev_type == "device" ? String() : dev.dev_type);
	put_if_non_empty(dd, "location_mode", dev.location_mode == "Local" ? String() : dev.location_mode);
	put_if_non_empty(dd, "remote_hmi_ip", dev.remote_hmi_ip);
	put_if_non_empty(dd, "interface_type", dev.interface_type);
	put_if_non_empty(dd, "ip", dev.ip);
	put_if_nonzero(dd, "port", dev.port);
	if (dev.use_udp) {
		dd["use_udp"] = true;
	}
	put_if_non_empty(dd, "serial_port", dev.serial_port);
	put_if_non_empty(dd, "baud_rate", dev.baud_rate);
	put_if_nonzero(dd, "data_bits", dev.data_bits);
	put_if_non_empty(dd, "parity", dev.parity);
	put_if_nonzero(dd, "stop_bits", dev.stop_bits);
	put_if_non_empty(dd, "flow_control", dev.flow_control);
	put_if_nonzero(dd, "station_no", dev.station_no);
	put_if_nonzero(dd, "broadcast_station_no", dev.broadcast_station_no);
	if (dev.use_station_variable) {
		dd["use_station_variable"] = true;
	}
	put_if_nonzero(dd, "timeout", dev.timeout);
	put_if_nonzero(dd, "comm_delay", dev.comm_delay);
	put_if_nonzero(dd, "retries", dev.retries);
	put_if_nonzero(dd, "max_read_words", dev.max_read_words);
	put_if_nonzero(dd, "max_write_words", dev.max_write_words);
	put_if_nonzero(dd, "poll_interval", dev.poll_interval);
	put_if_nonzero(dd, "block_size_words", dev.block_size_words);
	if (!dev.options.is_empty()) {
		dd["options"] = dev.options;
	}

	Array tags_arr;
	for (const auto &tag : dev.tags) {
		Dictionary td;
		td["name"] = tag.name;
		put_if_non_empty(td, "description", tag.description);

		const String schema = infer_tag_schema(tag);
		put_if_non_empty(td, "schema", schema);

		if (schema == "symbolic" || (!tag.symbol.is_empty() && tag.address_type.is_empty())) {
			put_if_non_empty(td, "symbol", tag.symbol);
			// Symbolic Auto/empty format: omit data_format; do not fall back to legacy TYPE_BOOL→"bit".
			put_if_non_empty(td, "data_format", tag.data_format);
		} else if (schema == "absolute" || !tag.address_type.is_empty()) {
			put_if_non_empty(td, "address_mode", tag.address_mode);
			put_if_non_empty(td, "address_type", tag.address_type);
			put_if_nonzero(td, "db_number", tag.db_number);
			put_if_non_empty(td, "address", tag.address);
			const String fmt = tag_wire_data_format(tag);
			put_if_non_empty(td, "data_format", fmt);
			put_if_nonzero(td, "length", tag.length);
		} else {
			put_if_non_empty(td, "address", tag.address);
			const String fmt = tag_wire_data_format(tag);
			put_if_non_empty(td, "data_format", fmt);
		}

		if (tag.writable) {
			td["writable"] = true;
		}
		put_if_nonzero(td, "poll_interval", tag.poll_interval);
		put_if_non_empty(td, "scan_group", tag.scan_group);
		put_if_non_empty(td, "unit", tag.unit);
		if (!tag.scale_obj.is_empty()) {
			td["scale"] = tag.scale_obj;
		}
		tags_arr.append(td);
	}
	dd["tags"] = tags_arr;
	return dd;
}

IndustrialTagData tag_from_dict(const Dictionary &p_td) {
	IndustrialTagData tag;
	tag.name = dict_get_string(p_td, "name");
	tag.description = dict_get_string(p_td, "description");
	tag.schema = dict_get_string(p_td, "schema");
	tag.address_mode = dict_get_string(p_td, "address_mode");
	tag.address_type = dict_get_string(p_td, "address_type");
	tag.data_format = dict_get_string(p_td, "data_format");
	tag.address = dict_get_string(p_td, "address");
	tag.db_number = dict_get_int(p_td, "db_number");
	tag.length = dict_get_int(p_td, "length");
	tag.symbol = dict_get_string(p_td, "symbol");
	tag.scan_group = dict_get_string(p_td, "scan_group");
	tag.writable = dict_get_bool(p_td, "writable");
	tag.poll_interval = dict_get_int(p_td, "poll_interval");
	tag.unit = dict_get_string(p_td, "unit");

	if (p_td.has("scale")) {
		Variant scale_v = p_td.get("scale", Variant());
		if (scale_v.get_type() == Variant::DICTIONARY) {
			tag.scale_obj = scale_v;
		} else if (scale_v.get_type() == Variant::INT || scale_v.get_type() == Variant::FLOAT) {
			tag.scale = scale_v;
		}
	}

	if (p_td.has("data_type")) {
		tag.data_type = dict_get_int(p_td, "data_type", TYPE_BOOL);
	}
	if (tag.data_format.is_empty() && p_td.has("data_type")) {
		tag.data_format = data_format_from_legacy_type(tag.data_type);
	} else if (!tag.data_format.is_empty()) {
		const int mapped = legacy_type_from_data_format(tag.data_format);
		if (mapped >= 0) {
			tag.data_type = mapped;
		}
	}

	if (tag.schema.is_empty()) {
		tag.schema = infer_tag_schema(tag);
	}
	return tag;
}

IndustrialDeviceData device_from_dict(const Dictionary &p_dd) {
	IndustrialDeviceData dev;
	dev.name = dict_get_string(p_dd, "name");
	if (dev.name.is_empty()) {
		dev.name = dict_get_string(p_dd, "id");
	}
	dev.description = dict_get_string(p_dd, "description");
	parse_driver_from_variant(p_dd.get("driver", 0), dev.driver, dev.driver_key);
	dev.enabled = dict_get_bool(p_dd, "enabled", true);
	dev.dev_type = dict_get_string(p_dd, "dev_type", "device");
	dev.location_mode = dict_get_string(p_dd, "location_mode", "Local");
	dev.remote_hmi_ip = dict_get_string(p_dd, "remote_hmi_ip");
	dev.interface_type = dict_get_string(p_dd, "interface_type");
	dev.ip = dict_get_string(p_dd, "ip");
	dev.port = dict_get_int(p_dd, "port");
	dev.use_udp = dict_get_bool(p_dd, "use_udp");
	dev.serial_port = dict_get_string(p_dd, "serial_port");
	dev.baud_rate = dict_get_string(p_dd, "baud_rate");
	dev.data_bits = dict_get_int(p_dd, "data_bits");
	dev.parity = dict_get_string(p_dd, "parity");
	dev.stop_bits = dict_get_int(p_dd, "stop_bits");
	dev.flow_control = dict_get_string(p_dd, "flow_control");
	dev.station_no = dict_get_int(p_dd, "station_no");
	dev.broadcast_station_no = dict_get_int(p_dd, "broadcast_station_no");
	dev.use_station_variable = dict_get_bool(p_dd, "use_station_variable");
	dev.timeout = dict_get_int(p_dd, "timeout");
	dev.comm_delay = dict_get_int(p_dd, "comm_delay");
	dev.retries = dict_get_int(p_dd, "retries");
	dev.max_read_words = dict_get_int(p_dd, "max_read_words");
	dev.max_write_words = dict_get_int(p_dd, "max_write_words");
	dev.poll_interval = dict_get_int(p_dd, "poll_interval");
	dev.block_size_words = dict_get_int(p_dd, "block_size_words");
	dev.scan_group = dict_get_string(p_dd, "scan_group");
	dev.options = p_dd.get("options", Dictionary());
	dev.connection_params = p_dd.get("connection_params", Dictionary());

	lift_connection_params(dev);
	sync_connection_params_from_flat(dev);

	Array tags_arr = p_dd.get("tags", Array());
	for (int j = 0; j < tags_arr.size(); j++) {
		Dictionary td = tags_arr[j];
		IndustrialTagData tag = tag_from_dict(td);
		if (!tag.name.is_empty()) {
			dev.tags.push_back(tag);
		}
	}
	return dev;
}

} // namespace

bool industrial_set_device_flat_key(IndustrialDeviceData &p_dev, const String &p_key, const Variant &p_value) {
	if (p_key == "ip") {
		p_dev.ip = String(p_value);
		return true;
	}
	if (p_key == "port") {
		p_dev.port = int(p_value);
		return true;
	}
	if (p_key == "use_udp") {
		p_dev.use_udp = bool(p_value);
		return true;
	}
	if (p_key == "serial_port") {
		p_dev.serial_port = String(p_value);
		return true;
	}
	if (p_key == "baud_rate") {
		p_dev.baud_rate = String(p_value);
		return true;
	}
	if (p_key == "data_bits") {
		p_dev.data_bits = int(p_value);
		return true;
	}
	if (p_key == "parity") {
		p_dev.parity = String(p_value);
		return true;
	}
	if (p_key == "stop_bits") {
		p_dev.stop_bits = int(p_value);
		return true;
	}
	if (p_key == "flow_control") {
		p_dev.flow_control = String(p_value);
		return true;
	}
	if (p_key == "station_no") {
		p_dev.station_no = int(p_value);
		return true;
	}
	if (p_key == "broadcast_station_no") {
		p_dev.broadcast_station_no = int(p_value);
		return true;
	}
	if (p_key == "use_station_variable") {
		p_dev.use_station_variable = bool(p_value);
		return true;
	}
	if (p_key == "timeout") {
		p_dev.timeout = int(p_value);
		return true;
	}
	if (p_key == "comm_delay") {
		p_dev.comm_delay = int(p_value);
		return true;
	}
	if (p_key == "retries") {
		p_dev.retries = int(p_value);
		return true;
	}
	if (p_key == "max_read_words") {
		p_dev.max_read_words = int(p_value);
		return true;
	}
	if (p_key == "max_write_words") {
		p_dev.max_write_words = int(p_value);
		return true;
	}
	if (p_key == "poll_interval") {
		p_dev.poll_interval = int(p_value);
		return true;
	}
	if (p_key == "block_size_words") {
		p_dev.block_size_words = int(p_value);
		return true;
	}
	if (p_key == "location_mode") {
		p_dev.location_mode = String(p_value);
		return true;
	}
	if (p_key == "remote_hmi_ip") {
		p_dev.remote_hmi_ip = String(p_value);
		return true;
	}
	if (p_key == "dev_type") {
		p_dev.dev_type = String(p_value);
		return true;
	}
	if (p_key == "interface_type") {
		p_dev.interface_type = String(p_value);
		return true;
	}
	return false;
}

Dictionary industrial_device_param_dict(const IndustrialDeviceData &p_dev) {
	Dictionary d;
	d["dev_type"] = p_dev.dev_type;
	d["location_mode"] = p_dev.location_mode;
	if (!p_dev.remote_hmi_ip.is_empty()) {
		d["remote_hmi_ip"] = p_dev.remote_hmi_ip;
	}
	if (!p_dev.interface_type.is_empty()) {
		d["interface_type"] = p_dev.interface_type;
	}
	if (!p_dev.ip.is_empty()) {
		d["ip"] = p_dev.ip;
	}
	if (p_dev.port != 0) {
		d["port"] = p_dev.port;
	}
	if (p_dev.use_udp) {
		d["use_udp"] = true;
	}
	if (!p_dev.serial_port.is_empty()) {
		d["serial_port"] = p_dev.serial_port;
	}
	if (!p_dev.baud_rate.is_empty()) {
		d["baud_rate"] = p_dev.baud_rate;
	}
	if (p_dev.data_bits != 0) {
		d["data_bits"] = p_dev.data_bits;
	}
	if (!p_dev.parity.is_empty()) {
		d["parity"] = p_dev.parity;
	}
	if (p_dev.stop_bits != 0) {
		d["stop_bits"] = p_dev.stop_bits;
	}
	if (!p_dev.flow_control.is_empty()) {
		d["flow_control"] = p_dev.flow_control;
	}
	if (p_dev.station_no != 0) {
		d["station_no"] = p_dev.station_no;
	}
	if (p_dev.broadcast_station_no != 0) {
		d["broadcast_station_no"] = p_dev.broadcast_station_no;
	}
	if (p_dev.use_station_variable) {
		d["use_station_variable"] = true;
	}
	if (p_dev.timeout != 0) {
		d["timeout"] = p_dev.timeout;
	}
	if (p_dev.comm_delay != 0) {
		d["comm_delay"] = p_dev.comm_delay;
	}
	if (p_dev.retries != 0) {
		d["retries"] = p_dev.retries;
	}
	if (p_dev.max_read_words != 0) {
		d["max_read_words"] = p_dev.max_read_words;
	}
	if (p_dev.max_write_words != 0) {
		d["max_write_words"] = p_dev.max_write_words;
	}
	if (p_dev.poll_interval != 0) {
		d["poll_interval"] = p_dev.poll_interval;
	}
	if (p_dev.block_size_words != 0) {
		d["block_size_words"] = p_dev.block_size_words;
	}

	Array opt_keys = p_dev.options.keys();
	for (int i = 0; i < opt_keys.size(); i++) {
		const String key = opt_keys[i];
		d[key] = p_dev.options[key];
	}

	// Compat: lift any legacy-only keys still sitting in connection_params.
	Array legacy_keys = p_dev.connection_params.keys();
	for (int i = 0; i < legacy_keys.size(); i++) {
		const String key = legacy_keys[i];
		if (!d.has(key)) {
			d[key] = p_dev.connection_params[key];
		}
	}
	return d;
}

void industrial_apply_param_dict_to_device(IndustrialDeviceData &p_dev, const Dictionary &p_params) {
	Dictionary merged_options = p_dev.options.duplicate(true);
	Array keys = p_params.keys();
	for (int i = 0; i < keys.size(); i++) {
		const String key = keys[i];
		const Variant value = p_params[key];
		if (industrial_set_device_flat_key(p_dev, key, value)) {
			continue;
		}
		merged_options[key] = value;
	}
	p_dev.options = merged_options;
}

void industrial_sync_device_connection_params(IndustrialDeviceData &p_dev) {
	p_dev.connection_params.clear();
	auto set_str = [&](const String &p_key, const String &p_value) {
		if (!p_value.is_empty()) {
			p_dev.connection_params[p_key] = p_value;
		}
	};
	auto set_int = [&](const String &p_key, int p_value) {
		if (p_value != 0) {
			p_dev.connection_params[p_key] = p_value;
		}
	};
	auto set_bool = [&](const String &p_key, bool p_value) {
		if (p_value) {
			p_dev.connection_params[p_key] = p_value;
		}
	};

	set_str("dev_type", p_dev.dev_type == "device" ? String() : p_dev.dev_type);
	set_str("location_mode", p_dev.location_mode == "Local" ? String() : p_dev.location_mode);
	set_str("remote_hmi_ip", p_dev.remote_hmi_ip);
	set_str("interface_type", p_dev.interface_type);
	set_str("ip", p_dev.ip);
	set_int("port", p_dev.port);
	set_bool("use_udp", p_dev.use_udp);
	set_str("serial_port", p_dev.serial_port);
	set_str("baud_rate", p_dev.baud_rate);
	set_int("data_bits", p_dev.data_bits);
	set_str("parity", p_dev.parity);
	set_int("stop_bits", p_dev.stop_bits);
	set_str("flow_control", p_dev.flow_control);
	set_int("station_no", p_dev.station_no);
	set_int("broadcast_station_no", p_dev.broadcast_station_no);
	set_bool("use_station_variable", p_dev.use_station_variable);
	set_int("timeout", p_dev.timeout);
	set_int("comm_delay", p_dev.comm_delay);
	set_int("retries", p_dev.retries);
	set_int("max_read_words", p_dev.max_read_words);
	set_int("max_write_words", p_dev.max_write_words);
	set_int("poll_interval", p_dev.poll_interval);
	set_int("block_size_words", p_dev.block_size_words);

	Array opt_keys = p_dev.options.keys();
	for (int i = 0; i < opt_keys.size(); i++) {
		const String key = opt_keys[i];
		p_dev.connection_params[key] = p_dev.options[key];
	}
}

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
	devices.remove_at(p_index);
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
	tags.remove_at(p_tag_index);
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
	scan_groups.remove_at(p_index);
	return true;
}

Dictionary IndustrialProject::to_dict() const {
	Dictionary result;

	if (!scan_groups.is_empty()) {
		Array groups_arr;
		for (const auto &g : scan_groups) {
			Dictionary gd;
			gd["name"] = g.name;
			gd["interval_ms"] = g.interval_ms;
			groups_arr.append(gd);
		}
		result["scan_groups"] = groups_arr;
	}

	Array devices_arr;
	for (const auto &dev : devices) {
		devices_arr.append(device_to_dict(dev));
	}
	result["devices"] = devices_arr;

	return result;
}

void IndustrialProject::from_dict(const Dictionary &p_data) {
	devices.clear();
	scan_groups.clear();

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

	Array devices_arr = p_data.get("devices", Array());
	for (int i = 0; i < devices_arr.size(); i++) {
		Dictionary dd = devices_arr[i];
		IndustrialDeviceData dev = device_from_dict(dd);
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
		if (dev.driver < 0) {
			if (!dev.driver_key.is_empty()) {
				errors.append(vformat(TTR("Device '%s' has unknown driver key '%s'."), dev.name, dev.driver_key));
			} else {
				errors.append(vformat(TTR("Device '%s' has invalid driver."), dev.name));
			}
		} else if (dev.driver >= industrial_get_driver_count()) {
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
			const bool has_symbolic = !tag.symbol.is_empty();
			const bool has_absolute = !tag.address_type.is_empty() || !tag.address_mode.is_empty();
			const bool has_legacy_address = !tag.address.is_empty();
			if (!has_symbolic && !has_absolute && !has_legacy_address) {
				errors.append(vformat(TTR("Tag '%s' on device '%s' has no address or schema fields."), tag.name, dev.name));
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
