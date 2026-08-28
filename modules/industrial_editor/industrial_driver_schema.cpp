#include "industrial_driver_schema.h"
#include "industrial_runtime_client.h"

#include "core/variant/variant.h"
#include "core/string/ustring.h"

static const DriverMeta s_driver_catalog_fallback[] = {
	{ "Siemens S7-1200/S7-1500", "siemens_s7", "Siemens", "ethernet", "absolute" }, // 0
	{ "Siemens S7-300 (Ethernet)", "siemens_s7_300_ethernet", "Siemens", "ethernet", "absolute" }, // 1
	{ "Siemens S7-400 (Ethernet)", "siemens_s7_400_ethernet", "Siemens", "ethernet", "absolute" }, // 2
	{ "Siemens S7-200 SMART (Ethernet)", "siemens_s7_200_smart_ethernet", "Siemens", "ethernet", "absolute" }, // 3
	{ "Siemens S7-200 (PPI)", "siemens_s7_200_ppi", "Siemens", "ppi", "absolute" }, // 4
	{ "Siemens S7-300 (MPI)", "siemens_s7_300_mpi", "Siemens", "mpi", "absolute" }, // 5
	{ "Siemens S7-300/400 (PROFIBUS DP-FMS)", "siemens_s7_300_400_profibus", "Siemens", "profibus", "absolute" }, // 6
	{ "Mitsubishi FX (Programming Port)", "mitsubishi_fx", "Mitsubishi", "serial_rs485", "absolute" }, // 7
	{ "Mitsubishi FX3U (Ethernet)", "mitsubishi_fx3u_ethernet", "Mitsubishi", "ethernet", "absolute" }, // 8
	{ "Mitsubishi FX5U (Ethernet)", "mitsubishi_fx5u_ethernet", "Mitsubishi", "ethernet", "absolute" }, // 9
	{ "Mitsubishi Q/L/R SLMP 3E Binary", "mitsubishi_q_smlp_3e_binary", "Mitsubishi", "ethernet", "absolute" }, // 10
	{ "Mitsubishi Q/L/R SLMP 4E Binary", "mitsubishi_q_smlp_4e_binary", "Mitsubishi", "ethernet", "absolute" }, // 11
	{ "Mitsubishi Q/L/R (Symbolic, GX Works3)", "mitsubishi_melsoft_tag", "Mitsubishi", "ethernet", "symbolic" }, // 12
	{ "Mitsubishi Q (MC 1E Frame, Serial)", "mitsubishi_q_mc_1e_frame", "Mitsubishi", "serial_rs485", "absolute" }, // 13
	{ "Mitsubishi QnA (MC 3C Frame, Serial)", "mitsubishi_qna_mc_3c_frame", "Mitsubishi", "serial_rs232c", "absolute" }, // 14
	{ "Omron CS/CJ/CP (FINS, Ethernet)", "omron_fins", "Omron", "ethernet", "absolute" }, // 15
	{ "Omron CV (FINS, Ethernet)", "omron_cv_fins_ethernet", "Omron", "ethernet", "absolute" }, // 16
	{ "Omron NX/NJ (EtherNet/IP Tag)", "omron_nj_nx_eip_tag", "Omron", "ethernet_ip", "symbolic" }, // 17
	{ "Omron (HostLink C-Mode)", "omron_hostlink_c_mode", "Omron", "ethernet", "absolute" }, // 18
	{ "Omron CV/C200HS (Toolbus)", "omron_cv_toolbus", "Omron", "ethernet", "absolute" }, // 19
	{ "Modbus TCP Client (Generic)", "modbus_tcp", "Modbus", "ethernet", "absolute" }, // 20
	{ "Modbus TCP Server (Gateway)", "modbus_tcp_server", "Modbus", "ethernet", "absolute" }, // 21
	{ "Modbus RTU (RS-485)", "modbus_rtu", "Modbus", "serial_rs485", "absolute" }, // 22
	{ "Modbus RTU (RS-232C)", "modbus_rtu_rs232c", "Modbus", "serial_rs232c", "absolute" }, // 23
	{ "Modbus ASCII", "modbus_ascii", "Modbus", "serial_rs485", "absolute" }, // 24
	{ "Delta DVP/AS Series Modbus TCP", "delta_modbus_tcp", "Delta", "ethernet", "absolute" }, // 25
	{ "Delta AH500 (EtherNet/IP)", "delta_ah500_eip", "Delta", "ethernet", "absolute" }, // 26
	{ "Keyence KV-8000 / KV Nano Modbus TCP", "keyence_kv8000_modbus_tcp", "Keyence", "ethernet", "absolute" }, // 27
	{ "Inovance H1U/H3U (Easy) Modbus TCP", "inovance_h3u_modbus_tcp", "Inovance", "ethernet", "absolute" }, // 28
	{ "Inovance H5U (Evo) Modbus TCP", "inovance_h5u_modbus_tcp", "Inovance", "ethernet", "absolute" }, // 29
	{ "Haiwell Modbus TCP", "haiwell_modbus_tcp", "Haiwell", "ethernet", "absolute" }, // 30
	{ "Fatek M Series Modbus TCP", "fatek_m_series_modbus_tcp", "Fatek", "ethernet", "absolute" }, // 31
	{ "Fatek FB Series (Fatek Bus, Ethernet)", "fatek_fb_modbus_tcp", "Fatek", "ethernet", "absolute" }, // 32
	{ "Schneider Modicon M340 Modbus TCP", "schneider_m340_modbus_tcp", "Schneider", "ethernet", "absolute" }, // 33
	{ "Schneider Modicon M580 (Tag-based)", "schneider_m580_tag_tcp", "Schneider", "ethernet", "symbolic" }, // 34
	{ "GE VersaMax Modbus TCP", "ge_versamax_modbus_tcp", "GE_Fanuc", "ethernet", "absolute" }, // 35
	{ "GE PACSystems RX3i (EtherNet/IP Tag)", "ge_pac_rx3i_eip_tag", "GE_Fanuc", "ethernet_ip", "symbolic" }, // 36
	{ "Artrich Inverter AR300 Modbus TCP", "artrich_ar300_modbus_tcp", "Artrich", "ethernet", "absolute" }, // 37
	{ "VEICHI Inverter Modbus TCP", "veichi_inverter_modbus_tcp", "Artrich", "ethernet", "absolute" }, // 38
	{ "MINGYANG Wind Turbine Modbus TCP", "mingyang_modbus_tcp", "Artrich", "ethernet", "absolute" }, // 39
	{ "Toshiba VF/TSB Inverter Modbus TCP", "toshiba_inverter_modbus_tcp", "Artrich", "ethernet", "absolute" }, // 40
	{ "Allen-Bradley ControlLogix/CompactLogix (EIP Class3 Tag)", "ab_controllogix_eip_class3_tag", "Rockwell", "ethernet_ip", "symbolic" }, // 41
	{ "Allen-Bradley MicroLogix/SLC-500 (DF1 Ethernet-encap)", "ab_micrologix_df1_ethernet_encap", "Rockwell", "ethernet", "absolute" }, // 42
	{ "Allen-Bradley MicroLogix/SLC-500 (DF1 Full-Duplex, Serial)", "ab_micrologix_df1_serial", "Rockwell", "serial_rs232c", "absolute" }, // 43
	{ "Beckhoff CX1000 TwinCAT 2 (ADS)", "beckhoff_cx1000_twincat2_ads", "Beckhoff", "ethernet", "absolute" }, // 44
	{ "Beckhoff CX5xxx/CX9xxx Embedded PC (ADS)", "beckhoff_cx_embedded_ads", "Beckhoff", "ethernet", "symbolic" }, // 45
	{ "Beckhoff TwinCAT 3 Project (Tag Import)", "beckhoff_twincat3_tag", "Beckhoff", "ethernet", "symbolic" }, // 46
	{ "Automation Direct Click/Nano Modbus TCP", "automation_direct_modbus_tcp", "Artrich", "ethernet", "absolute" }, // 47
	{ "Panasonic FP0/FP2/FP-X/FP7 (MEWTOCOL-7)", "panasonic_mewtocol7_ethernet", "Panasonic", "ethernet", "absolute" }, // 48
	{ "IDEC FC/FZ Series Modbus TCP", "idec_fc_fz_modbus_tcp", "IDEC", "ethernet", "absolute" }, // 49
	{ "Crouzet EM4 Millenium 3 Modbus TCP", "crouzet_em4_modbus_tcp", "Crouzet", "ethernet", "absolute" }, // 50
	{ "Emerson ROC800 (ROC Plus Protocol)", "emerson_roc800", "Emerson", "ethernet", "absolute" }, // 51
	{ "Emerson (Lust) ServoOne Modbus TCP", "lust_servoone_modbus_tcp", "Artrich", "ethernet", "absolute" }, // 52
	{ "Sigmatek DIAS-TCP", "sigmatek_dias_ethernet", "Sigmatek", "ethernet", "absolute" }, // 53
	{ "Schleicher Unipro Bridge", "schleicher_unipro_ethernet", "Schleicher", "ethernet", "absolute" }, // 54
	{ "SICK Barcode Reader / 3D Camera Modbus TCP", "sick_modbus_tcp", "SICK", "ethernet", "absolute" }, // 55
	{ "Hollysys (Symbolic)", "hollysys_symbolic_ethernet", "Hollysys", "ethernet", "symbolic" }, // 56
	{ "Nova LED Control Card (Symbolic)", "novastar_symbolic_ethernet", "Nova", "ethernet", "symbolic" }, // 57
	{ "Kontar PLC (Ethernet)", "kontar_plc_ethernet", "Kontar", "ethernet", "absolute" }, // 58
	{ "ProCon Protocol (Ethernet)", "procon_protocol_ethernet", "Procon", "ethernet", "absolute" }, // 59
	{ "EtherCAT Master (SDO/PDO)", "ethercat_master", "EtherCAT", "ethercat", "absolute" }, // 60
	{ "BACnet MS/TP (RS-485)", "bacnet_mstp", "BACnet", "bacnet_mstp", "absolute" }, // 61
	{ "BACnet/IP (Annex J UDP 47808)", "bacnet_ip", "BACnet", "bacnet_ip", "absolute" }, // 62
	{ "IEC 60870-5-104 (APDU TCP 2404)", "iec_104", "IEC", "iec_104", "absolute" }, // 63
	{ "IEC 60870-5-101 (Serial)", "iec_101", "IEC", "iec_101", "absolute" }, // 64
	{ "DNP3 Serial", "dnp3_serial", "DNP3", "serial_rs232c", "absolute" }, // 65
	{ "DNP3 over TCP", "dnp3_tcp", "DNP3", "ethernet", "absolute" }, // 66
	{ "SECS/GEM Host (HSMS Active/Passive)", "secs_gem_hsms", "SECS", "hsms", "absolute" }, // 67
	{ "SECS-I (RS-232C)", "secs_i_serial", "SECS", "hsms", "absolute" }, // 68
	{ "SAE J1939 (CANbus 250/500kbps)", "j1939_can_symbolic", "SAE", "can_j1939", "symbolic" }, // 69
	{ "Free Protocol (Raw TCP)", "free_protocol_tcp", "FreeProtocol", "ethernet", "absolute" }, // 70
	{ "Free Protocol (Raw UDP)", "free_protocol_udp", "FreeProtocol", "ethernet", "absolute" }, // 71
	{ "Free Protocol (Raw Serial RS-232C)", "free_protocol_serial_rs232c", "FreeProtocol", "serial_rs232c", "absolute" }, // 72
	{ "Free Protocol (Raw Serial RS-485)", "free_protocol_serial_rs485", "FreeProtocol", "serial_rs485", "absolute" }, // 73
	{ "Weintek Remote IO (cMT-G01 / iR IO)", "weintek_remote_io_tcp", "Weintek", "ethernet", "absolute" }, // 74
	{ "Delta DVP/ES/SS Modbus RTU", "delta_modbus_rtu", "Delta", "serial_rs485", "absolute" }, // 75
	{ "Haiwell Modbus RTU", "haiwell_modbus_rtu", "Haiwell", "serial_rs485", "absolute" }, // 76
	{ "Fatek FBs/FB (Fatek Serial)", "fatek_fb_serial", "Fatek", "serial_rs485", "absolute" }, // 77
	{ "Inovance H1U/H3U Modbus RTU", "inovance_h3u_modbus_rtu", "Inovance", "serial_rs485", "absolute" }, // 78
	{ "Crouzet EM4 Modbus RTU", "crouzet_em4_modbus_rtu", "Crouzet", "serial_rs485", "absolute" }, // 79
	{ "IDEC Modbus RTU", "idec_modbus_rtu", "IDEC", "serial_rs485", "absolute" }, // 80
};
static_assert(sizeof(s_driver_catalog_fallback) / sizeof(s_driver_catalog_fallback[0]) == 81);
static constexpr int kFallbackDriverCount = 81;

static const char *data_type_names[] = {
	"Bool",
	"Int16",
	"Int32",
	"UInt16",
	"UInt32",
	"Byte",
	"Float32",
	"Float64",
	"BCD16",
	"BCD32",
	"Real32 (IEEE754)",
	"Real64 (IEEE754)",
};
static_assert(sizeof(data_type_names) / sizeof(data_type_names[0]) == 12);

String industrial_get_driver_name(int p_driver) {
	// Prefer the canonical backend-supplied display name (via
	// GET /api/v1/drivers).  When the Go runtime is not reachable we fall
	// back to the legacy hard-coded list so the form never goes blank.
	String from_runtime = IndustrialRuntimeClient::get_driver_name(p_driver);
	if (!from_runtime.is_empty()) {
		return from_runtime;
	}
	if (p_driver < 0 || p_driver >= kFallbackDriverCount) {
		return "Unknown";
	}
	return String(s_driver_catalog_fallback[p_driver].display_name);
}

String industrial_get_data_type_name(int p_type) {
	if (p_type < 0 || p_type >= 12) {
		return "Unknown";
	}
	return String(data_type_names[p_type]);
}

StringList industrial_get_data_type_names() {
	StringList result;
	for (int i = 0; i < 12; i++) {
		result.push_back(data_type_names[i]);
	}
	return result;
}

StringList industrial_get_driver_names() {
	// Prefer backend list (can grow beyond legacy 7; still iterate by index).
	int backend_count = IndustrialRuntimeClient::get_driver_count();
	int count = backend_count > kFallbackDriverCount ? backend_count : kFallbackDriverCount;
	StringList result;
	for (int i = 0; i < count; i++) {
		result.push_back(industrial_get_driver_name(i));
	}
	return result;
}

// --- S7 address type catalog ---------------------------------------------
// The canonical list comes from the Go runtime HTTP API via
// IndustrialRuntimeClient::get_s7_address_types(), which fetches from:
//   GET /api/v1/drivers/s7/address-catalog
// returning 235 entries (base 31 + DB1-99 + DB1Bit-99Bit + S5TIME timers).
//
// If the Go runtime is not reachable (e.g. editor launched without it), the
// client falls back to a hard-coded list covering the most common types.

StringList industrial_get_s7_address_types() {
	PackedStringArray types = IndustrialRuntimeClient::get_s7_address_types();
	StringList result;
	for (int i = 0; i < types.size(); i++) {
		result.push_back(types[i]);
	}
	return result;
}

// All field.name / field.tooltip / choice strings below use English
// placeholders wrapped in TTRC() so they are extracted into the .pot template
// and rendered via Godot's translation system at runtime.  This bypasses the
// Windows-locale Latin-1 misdecoding that occurs when raw UTF-8 CJK literals
// are passed to String(const char*).  Label::set_text() and OptionButton
// both call atr() internally, so the English msgid is automatically replaced
// with the Chinese translation from zh_Hans.po / zh_Hant.po.

#define FIELD_NAME(_s) (String::utf8(TTRC(_s)))
#define FIELD_TIP(_s)  (String::utf8(TTRC(_s)))
#define FIELD_CHOICE(_s) (String::utf8(TTRC(_s)))

static Vector<IndustrialFieldDef> make_common_fields() {
	return {
		{ FIELD_NAME("Poll Interval (ms)"),  "poll_interval", 0, Variant(500), 10, 60000, {}, FIELD_TIP("Poll Interval") },
		{ FIELD_NAME("Timeout (ms)"),        "timeout",       0, Variant(1000), 100, 60000, {}, FIELD_TIP("Request Timeout") },
		{ FIELD_NAME("Retries"),             "retries",       0, Variant(3),    0, 10,      {}, FIELD_TIP("Retry Count on Failure") },
	};
}

Vector<IndustrialFieldDef> industrial_get_driver_fields(int p_driver) {
	// Priority 1: live driver schema from Go runtime (GET /api/v1/drivers).
	// The backend is considered authoritative — it already carries common
	// fields (poll_interval / timeout / retries) plus driver-specific ones.
	{
		Vector<IndustrialFieldDef> from_runtime =
			IndustrialRuntimeClient::get_driver_fields(p_driver);
		if (from_runtime.size() > 0) {
			return from_runtime;
		}
	}

	// Priority 2 (fallback): local hardcoded schema — kept in parallel so
	// the New Device dialog remains usable even when Go runtime isn't
	// started.  This MUST stay in sync with the Go-side catalog in
	// internal/api/http.go `driverCatalogData`.
	Vector<IndustrialFieldDef> fields = make_common_fields();

	switch (p_driver) {
		case 0: case 1: case 2: case 3: { // Siemens S7 Ethernet variants
			fields.push_back({ FIELD_NAME("Interface Type"),        "interface_type", 4, Variant(FIELD_CHOICE("Ethernet")), 0, 0,     { FIELD_CHOICE("Ethernet"), FIELD_CHOICE("MPI"), FIELD_CHOICE("PROFIBUS") }, String() });
			fields.push_back({ FIELD_NAME("IP Address"),           "ip",             2, Variant("192.168.0.1"), 0, 0,     {}, String() });
			fields.push_back({ FIELD_NAME("Port"),                 "port",           0, Variant(102),           1, 65535, {}, FIELD_TIP("RFC1006 Default Port 102") });
			fields.push_back({ FIELD_NAME("Rack"),                 "rack",           0, Variant(0),             0, 7,     {}, FIELD_TIP("S7 CPU Rack, Usually 0") });
			fields.push_back({ FIELD_NAME("Slot"),                 "slot",           0, Variant(1),             0, 31,    {}, FIELD_TIP("S7-1500=1; S7-1200 CPU=0/1; Check Manual for Comm Module") });
			fields.push_back({ FIELD_NAME("Connection Type (TSAP)"), "tsap_mode",    4, Variant(FIELD_CHOICE("S7 Basic")), 0, 0,     { FIELD_CHOICE("PG"), FIELD_CHOICE("OP/HMI"), FIELD_CHOICE("S7 Basic") }, FIELD_TIP("TIA Portal: Enable PUT/GET Communication for S7 Basic") });
			fields.push_back({ FIELD_NAME("Max Read Words"),       "max_read_words", 0, Variant(220),           1, 2048,  {}, FIELD_TIP("Single S7 Read PDU Limit; Auto-Split if Exceeded") });
			fields.push_back({ FIELD_NAME("Max Write Words"),      "max_write_words",0, Variant(200),           1, 2048,  {}, String() });
		} break;
		case 4: { // Siemens S7-200 (PPI) — serial
			fields.push_back({ FIELD_NAME("Serial Port"), "serial_port", 2, Variant("COM1"), 0, 0, {}, String() });
			fields.push_back({ FIELD_NAME("Baud Rate"), "baud_rate", 4, Variant("9600"), 0, 0, {"9600", "19200", "38400", "115200"}, String() });
			fields.push_back({ FIELD_NAME("Station Address"), "slave_id", 0, Variant(2), 0, 254, {}, String() });
		} break;
		case 5: { // Siemens S7-300 (MPI) — serial
			fields.push_back({ FIELD_NAME("Serial Port"), "serial_port", 2, Variant("COM1"), 0, 0, {}, String() });
			fields.push_back({ FIELD_NAME("Baud Rate"), "baud_rate", 4, Variant("18750"), 0, 0, {"9600", "19200", "18750", "37500", "50000"}, String() });
			fields.push_back({ FIELD_NAME("MPI Address"), "slave_id", 0, Variant(2), 0, 126, {}, String() });
		} break;
		case 6: { // Siemens S7-300/400 (PROFIBUS) — bus
			fields.push_back({ FIELD_NAME("Baud Rate"), "baud_rate", 4, Variant("187.5k"), 0, 0, {"9.6k", "19.2k", "187.5k", "500k", "1.5M", "3M", "6M", "12M"}, String() });
			fields.push_back({ FIELD_NAME("Profibus Address"), "slave_id", 0, Variant(2), 0, 126, {}, String() });
		} break;
		case 7: { // Mitsubishi FX (Programming Port)
			fields.push_back({ FIELD_NAME("Interface"),      "interface",  4, Variant("COM1"),  0, 0,     {"COM1", "COM2", FIELD_CHOICE("Ethernet")}, String() });
			fields.push_back({ FIELD_NAME("Baud Rate"),    "baud_rate",  4, Variant("9600"),  0, 0,     {"9600", "19200", "38400", "115200"}, String() });
			fields.push_back({ FIELD_NAME("Slave ID"),      "slave_id",   0, Variant(0),       0, 31,    {}, String() });
		} break;
		case 15: { // Omron CS/CJ/CP (FINS, Ethernet)
			fields.push_back({ FIELD_NAME("Comm Mode"),   "comm_mode",    4, Variant(FIELD_CHOICE("Ethernet")), 0, 0,     { FIELD_CHOICE("Ethernet"), "RS-232C" }, String() });
			fields.push_back({ FIELD_NAME("IP Address"),    "ip",           2, Variant("192.168.0.1"),   0, 0,     {}, String() });
			fields.push_back({ FIELD_NAME("Node Address"),   "node_address", 0, Variant(0),               0, 254,   {}, String() });
			fields.push_back({ FIELD_NAME("Port"),           "port",         0, Variant(9600),            1, 65535, {}, String() });
		} break;
		case 20: { // Modbus TCP Client (Generic)
			fields.push_back({ FIELD_NAME("IP Address"), "ip",          2, Variant("192.168.0.1"), 0, 0,     {}, String() });
			fields.push_back({ FIELD_NAME("Port"),      "port",        0, Variant(502),             1, 65535, {}, String() });
			fields.push_back({ FIELD_NAME("Slave ID"),  "slave_id",    0, Variant(1),               1, 247,   {}, String() });
		} break;
		case 22: { // Modbus RTU (RS-485)
			fields.push_back({ FIELD_NAME("Serial Port"),   "serial_port", 2, Variant("COM1"),  0, 0,     {}, String() });
			fields.push_back({ FIELD_NAME("Baud Rate"),     "baud_rate",   4, Variant("9600"),  0, 0,     {"1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"}, String() });
			fields.push_back({ FIELD_NAME("Data Bits"),     "data_bits",   4, Variant("8"),     0, 0,     {"5", "6", "7", "8"}, String() });
			fields.push_back({ FIELD_NAME("Stop Bits"),     "stop_bits",   4, Variant("1"),     0, 0,     {"1", "2"}, String() });
			fields.push_back({ FIELD_NAME("Parity"),        "parity",      4, Variant("None"),  0, 0,     {"None", "Even", "Odd"}, String() });
			fields.push_back({ FIELD_NAME("Slave ID"),      "slave_id",    0, Variant(1),       1, 247,   {}, String() });
		} break;
		case 61: { // BACnet MS/TP (RS-485)
			fields.push_back({ FIELD_NAME("MAC Address"),   "mac_address",      2, Variant("00:00:00:00:00:00"), 0, 0,     {}, String() });
			fields.push_back({ FIELD_NAME("Baud Rate"),     "baud_rate",        4, Variant("9600"),              0, 0,     {"9600", "19200", "38400", "76800"}, String() });
			fields.push_back({ FIELD_NAME("Network Number"), "network_number",   0, Variant(0),                   0, 65535, {}, String() });
			fields.push_back({ FIELD_NAME("Device ID"),    "device_id",        0, Variant(0),                   0, 4194303, {}, String() });
		} break;
		case 67: { // SECS/GEM Host (HSMS Active/Passive)
			fields.push_back({ FIELD_NAME("Mode"),          "mode",         4, Variant(FIELD_CHOICE("Passive")), 0, 0,      { FIELD_CHOICE("Passive"), FIELD_CHOICE("Active") }, String() });
			fields.push_back({ FIELD_NAME("IP Address"),   "ip",           2, Variant("127.0.0.1"),       0, 0,      {}, String() });
			fields.push_back({ FIELD_NAME("Port"),          "port",         0, Variant(5000),              1, 65535,  {}, String() });
			fields.push_back({ FIELD_NAME("T3 Timeout (ms)"),  "t3_timeout",   0, Variant(10000),             100, 60000,{}, String() });
			fields.push_back({ FIELD_NAME("T5 Timeout (ms)"),  "t5_timeout",   0, Variant(1000),              100, 60000,{}, String() });
		} break;
		default: break;
	}

	return fields;
}

// --- New driver metadata accessors ----------------------------------------

const DriverMeta *industrial_get_driver_meta(int p_driver) {
	if (p_driver < 0 || p_driver >= kFallbackDriverCount) {
		return nullptr;
	}
	return &s_driver_catalog_fallback[p_driver];
}

String industrial_get_driver_key(int p_driver) {
	if (p_driver < 0 || p_driver >= kFallbackDriverCount) {
		return String();
	}
	return String(s_driver_catalog_fallback[p_driver].driver_key);
}

int industrial_find_driver_index_by_key(const String &p_key) {
	if (p_key.is_empty()) {
		return -1;
	}
	const int count = industrial_get_driver_count();
	for (int i = 0; i < count; i++) {
		if (industrial_get_driver_key(i) == p_key) {
			return i;
		}
	}
	return -1;
}

int industrial_get_driver_count() {
	int backend_count = IndustrialRuntimeClient::get_driver_count();
	return backend_count > kFallbackDriverCount ? backend_count : kFallbackDriverCount;
}
