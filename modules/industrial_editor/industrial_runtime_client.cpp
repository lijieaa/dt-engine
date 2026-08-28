#include "industrial_runtime_client.h"

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/io/http_client.h"
#include "core/object/class_db.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "core/string/print_string.h"
#include "scene/main/http_request.h"

#include "industrial_driver_schema.h"   // IndustrialFieldDef

#ifdef TOOLS_ENABLED
// EditorSettings 仅在 editor target 可用。头文件位于 editor/settings/。
#  include "editor/settings/editor_settings.h"
#endif

// --- Static members --------------------------------------------------------

HTTPRequest *IndustrialRuntimeClient::s_http_drivers   = nullptr;
HTTPRequest *IndustrialRuntimeClient::s_http_s7_addr   = nullptr;
int          IndustrialRuntimeClient::s_pending_requests = 0;

bool IndustrialRuntimeClient::s_driver_catalog_ready  = false;
bool IndustrialRuntimeClient::s_address_catalog_ready = false;

PackedStringArray IndustrialRuntimeClient::s_s7_address_types;
Array IndustrialRuntimeClient::s_s7_address_catalog;
Array IndustrialRuntimeClient::s_driver_catalog;
Callable IndustrialRuntimeClient::s_fetch_callback;
Callable IndustrialRuntimeClient::s_create_project_success_callback;

// --- Bind methods ----------------------------------------------------------

void IndustrialRuntimeClient::_bind_methods() {
	ClassDB::bind_static_method(
		"IndustrialRuntimeClient",
		D_METHOD("fetch_metadata", "owner"),
		&IndustrialRuntimeClient::fetch_metadata);
	ClassDB::bind_static_method(
		"IndustrialRuntimeClient",
		D_METHOD("is_address_catalog_ready"),
		&IndustrialRuntimeClient::is_address_catalog_ready);
	ClassDB::bind_static_method(
		"IndustrialRuntimeClient",
		D_METHOD("get_s7_address_types"),
		&IndustrialRuntimeClient::get_s7_address_types);
	ClassDB::bind_static_method(
		"IndustrialRuntimeClient",
		D_METHOD("get_s7_address_catalog"),
		&IndustrialRuntimeClient::get_s7_address_catalog);
	ClassDB::bind_static_method(
		"IndustrialRuntimeClient",
		D_METHOD("get_driver_catalog"),
		&IndustrialRuntimeClient::get_driver_catalog);
	ClassDB::bind_static_method(
		"IndustrialRuntimeClient",
		D_METHOD("get_driver_count"),
		&IndustrialRuntimeClient::get_driver_count);
	ClassDB::bind_static_method(
		"IndustrialRuntimeClient",
		D_METHOD("get_driver_name", "index"),
		&IndustrialRuntimeClient::get_driver_name);
	ClassDB::bind_static_method(
		"IndustrialRuntimeClient",
		D_METHOD("get_driver_key", "index"),
		&IndustrialRuntimeClient::get_driver_key);
	ClassDB::bind_static_method(
		"IndustrialRuntimeClient",
		D_METHOD("set_fetch_callback", "callback"),
		&IndustrialRuntimeClient::set_fetch_callback);
}

// --- Helpers ---------------------------------------------------------------

namespace {
String _normalize(String p_url) {
	while (p_url.length() > 0 && p_url[p_url.length() - 1] == '/') {
		p_url = p_url.substr(0, p_url.length() - 1);
	}
	return p_url;
}

// Translates the Go "kind" enum string → legacy industrial_data_type integer:
//   0 = INT (SpinBox number)
//   1 = FLOAT (unused, but keep for forward compat)
//   2 = TEXT  (LineEdit)
//   3 = BOOL  (CheckBox, currently unused)
//   4 = CHOICE (OptionButton)
int _kind_to_int(const String &p_kind) {
	if (p_kind == "int")    return 0;
	if (p_kind == "float")  return 1;
	if (p_kind == "text")   return 2;
	if (p_kind == "bool")   return 3;
	if (p_kind == "choice") return 4;
	return 2;   // safest fallback
}

// Converts a single field Dictionary coming from the backend JSON into a
// legacy IndustrialFieldDef struct.  Translation (TTRC) is handled by the
// consumer side (Label::set_text / OptionButton::add_item call atr() so the
// English msgid gets replaced at render time).
IndustrialFieldDef _dict_to_field_def(const Dictionary &p_d) {
	IndustrialFieldDef f;
	String label   = p_d.get("label_msgid",   Variant(""));
	String key     = p_d.get("key",           Variant(""));
	String kind    = p_d.get("kind",          Variant("text"));
	String tooltip = p_d.get("tooltip_msgid", Variant(""));
	Variant defv   = p_d.get("default_value", Variant());
	double min_v   = p_d.get("min_value",     0.0);
	double max_v   = p_d.get("max_value",     0.0);
	int max_len    = p_d.get("max_length",    0);
	Array choices  = p_d.get("choice_labels", Array());

	f.name          = label;       // English msgid; TTRC applied via atr() on display
	f.key           = key;
	f.data_type     = _kind_to_int(kind);
	f.default_value = defv;
	f.min_value     = min_v;
	f.max_value     = max_v;
	f.max_length    = max_len;
	f.tooltip       = tooltip;
	for (int i = 0; i < choices.size(); i++) {
		String c = choices[i];
		if (c.length() > 0) {
			f.choices.push_back(c);
		}
	}
	return f;
}
}  // namespace

String IndustrialRuntimeClient::get_runtime_url() {
	// 优先级严格按文档：命令行 > 环境变量 > ProjectSettings > EditorSettings > 默认。
	// 导出后的打包程序：仍有 1/2/3/5 四层可配置。

	// ---------- (1) Command line: --runtime-url=http://host:port ----------
	{
		List<String> args = OS::get_singleton()->get_cmdline_args();
		String prefix = kCmdlineFlag;
		for (const String &a : args) {
			if (a.begins_with(prefix)) {
				String val = a.substr(prefix.length());
				if (val.length() > 0) {
					return _normalize(val);
				}
			}
		}
	}

	// ---------- (2) Environment: GD_RUNTIME_URL ----------
	{
		String env_url = OS::get_singleton()->get_environment(kEnvVar);
		if (env_url.length() > 0) {
			return _normalize(env_url);
		}
	}

	// ---------- (3) ProjectSettings: industrial/runtime/url ----------
	{
		ProjectSettings *ps = ProjectSettings::get_singleton();
		if (ps != nullptr && ps->has_setting(kProjectSettingKey)) {
			Variant v = ps->get_setting(kProjectSettingKey);
			if (v.get_type() == Variant::STRING) {
				String s = v;
				if (s.length() > 0) {
					return _normalize(s);
				}
			}
		}
	}

	// ---------- (4) EditorSettings: industrial/runtime/url (editor only) ---
#ifdef TOOLS_ENABLED
	{
		EditorSettings *es = EditorSettings::get_singleton();
		if (es != nullptr && es->has_setting(kEditorSettingKey)) {
			Variant v = es->get_setting(kEditorSettingKey);
			if (v.get_type() == Variant::STRING) {
				String s = v;
				if (s.length() > 0) {
					return _normalize(s);
				}
			}
		}
	}
#endif

	// ---------- (5) Default fallback ----------
	return kDefaultUrl;
}

// --- Fallback data ---------------------------------------------------------

PackedStringArray IndustrialRuntimeClient::s7_address_types_fallback() {
	// Matches the 23-type subset returned by GET /api/v1/drivers/s7/address-catalog
	// (see internal/api/http.go s7AddressCatalog for whitelist + canonical order).
	PackedStringArray result;
	result.append("IB"); result.append("IW"); result.append("ID");
	result.append("QB"); result.append("QW"); result.append("QD");
	result.append("MB"); result.append("MW"); result.append("MD");
	result.append("DBBn"); result.append("DBBx");
	result.append("DBn");  result.append("DBx");
	result.append("DBDn"); result.append("DBDx");
	result.append("DBn_String");  result.append("DBx_String");
	result.append("DBn_String1"); result.append("DBx_String1");
	result.append("DBn_WString"); result.append("DBx_WString");
	result.append("DBDn_String"); result.append("DBDx_String");
	return result;
}

// --- Public API ------------------------------------------------------------

void IndustrialRuntimeClient::fetch_metadata(Node *p_owner) {
	ERR_FAIL_NULL(p_owner);

	String base_url = get_runtime_url();
	print_line(vformat("industrial_runtime: base URL = %s", base_url));

	// -------- GET /api/v1/drivers (driver names + field schemas) --------
	if (s_http_drivers == nullptr) {
		s_http_drivers = memnew(HTTPRequest);
		s_http_drivers->set_timeout(10.0);
		p_owner->add_child(s_http_drivers);
		s_http_drivers->connect(
			"request_completed",
			callable_mp_static(&IndustrialRuntimeClient::_on_drivers_completed));
	}
	// -------- GET /api/v1/drivers/s7/address-catalog (23 type names) --------
	if (s_http_s7_addr == nullptr) {
		s_http_s7_addr = memnew(HTTPRequest);
		s_http_s7_addr->set_timeout(10.0);
		p_owner->add_child(s_http_s7_addr);
		s_http_s7_addr->connect(
			"request_completed",
			callable_mp_static(&IndustrialRuntimeClient::_on_s7_addr_completed));
	}

	// Start both requests concurrently.  Each callback decrements
	// s_pending_requests; when it reaches 0 the cache is marked ready and
	// s_fetch_callback fires once (only once).
	s_pending_requests = 0;

	{
		String url = base_url + "/api/v1/drivers";
		s_pending_requests++;
		Error err = s_http_drivers->request(url);
		if (err != OK) {
			print_line(vformat("industrial_runtime: drivers request failed (%d)", err));
			_on_drivers_completed(err, 0, PackedStringArray(), PackedByteArray());
		} else {
			print_line(vformat("industrial_runtime: GET %s", url));
		}
	}
	{
		String url = base_url + "/api/v1/drivers/s7/address-catalog";
		s_pending_requests++;
		Error err = s_http_s7_addr->request(url);
		if (err != OK) {
			print_line(vformat("industrial_runtime: S7 addr request failed (%d)", err));
			_on_s7_addr_completed(err, 0, PackedStringArray(), PackedByteArray());
		} else {
			print_line(vformat("industrial_runtime: GET %s", url));
		}
	}
}

bool IndustrialRuntimeClient::is_address_catalog_ready() {
	return s_address_catalog_ready;
}

PackedStringArray IndustrialRuntimeClient::get_s7_address_types() {
	if (s_s7_address_types.size() > 0) {
		return s_s7_address_types;
	}
	return s7_address_types_fallback();
}

Array IndustrialRuntimeClient::get_s7_address_catalog() {
	return s_s7_address_catalog;
}

Array IndustrialRuntimeClient::get_driver_catalog() {
	return s_driver_catalog;
}

int IndustrialRuntimeClient::get_driver_count() {
	return s_driver_catalog.size();
}

String IndustrialRuntimeClient::get_driver_name(int p_driver) {
	if (p_driver < 0 || p_driver >= s_driver_catalog.size()) {
		return String();
	}
	Dictionary d = s_driver_catalog[p_driver];
	return d.get("display_name", Variant(""));
}

String IndustrialRuntimeClient::get_driver_key(int p_driver) {
	if (p_driver < 0 || p_driver >= s_driver_catalog.size()) {
		return String();
	}
	Dictionary d = s_driver_catalog[p_driver];
	return d.get("driver_key", Variant(""));
}

String IndustrialRuntimeClient::get_driver_vendor(int p_driver) {
	if (p_driver < 0 || p_driver >= s_driver_catalog.size()) return String();
	Dictionary d = s_driver_catalog[p_driver];
	return d.get("category_vendor", Variant(""));
}

String IndustrialRuntimeClient::get_driver_interface(int p_driver) {
	if (p_driver < 0 || p_driver >= s_driver_catalog.size()) return String();
	Dictionary d = s_driver_catalog[p_driver];
	return d.get("category_interface", Variant(""));
}

String IndustrialRuntimeClient::get_driver_addressing_mode(int p_driver) {
	if (p_driver < 0 || p_driver >= s_driver_catalog.size()) return String();
	Dictionary d = s_driver_catalog[p_driver];
	return d.get("addressing_mode", Variant(""));
}

bool IndustrialRuntimeClient::get_driver_supports_tag_import(int p_driver) {
	if (p_driver < 0 || p_driver >= s_driver_catalog.size()) return false;
	Dictionary d = s_driver_catalog[p_driver];
	return (bool)d.get("supports_tag_import", false);
}

bool IndustrialRuntimeClient::get_driver_supports_udp(int p_driver) {
	if (p_driver < 0 || p_driver >= s_driver_catalog.size()) return false;
	Dictionary d = s_driver_catalog[p_driver];
	return (bool)d.get("supports_udp", false);
}

bool IndustrialRuntimeClient::get_driver_supports_perf_params(int p_driver) {
	if (p_driver < 0 || p_driver >= s_driver_catalog.size()) return false;
	Dictionary d = s_driver_catalog[p_driver];
	return (bool)d.get("supports_perf_params", false);
}

Vector<IndustrialFieldDef> IndustrialRuntimeClient::get_driver_fields(int p_driver) {
	Vector<IndustrialFieldDef> out;
	if (p_driver < 0 || p_driver >= s_driver_catalog.size()) {
		return out;
	}
	Dictionary driver = s_driver_catalog[p_driver];
	Array fields = driver.get("fields", Array());
	for (int i = 0; i < fields.size(); i++) {
		Dictionary fd = fields[i];
		out.push_back(_dict_to_field_def(fd));
	}
	return out;
}

void IndustrialRuntimeClient::set_fetch_callback(Callable p_callback) {
	s_fetch_callback = p_callback;
}

// --- Internal helpers: shared JSON parsing + dual-request settlement -------

Dictionary IndustrialRuntimeClient::_parse_response_body(
		const PackedByteArray &p_body, String *r_error) {
	// Step 3: PackedByteArray → String via UTF-8 (same as EngineUpdateLabel).
	const uint8_t *r = p_body.ptr();
	String body = String::utf8((const char *)r, p_body.size());

	// Step 4: Parse JSON
	Variant parsed = JSON::parse_string(body);
	if (parsed == Variant()) {
		if (r_error) *r_error = "JSON parse failed";
		return Dictionary();
	}
	if (parsed.get_type() != Variant::DICTIONARY) {
		if (r_error) *r_error = "root is not a JSON object";
		return Dictionary();
	}
	return Dictionary(parsed);
}

void IndustrialRuntimeClient::_maybe_settle() {
	// Decrement first (both callbacks call this exactly once per request).
	if (s_pending_requests > 0) {
		s_pending_requests--;
	}
	if (s_pending_requests != 0) {
		return;
	}
	// Both requests settled.  Fire callback once.
	if (s_fetch_callback.is_valid()) {
		Array info;
		info.append(s_driver_catalog.size());
		info.append(s_s7_address_types.size());
		// Errors during a request will have already left *_catalog_ready=true
		// via the fallback branch; we don't surface them here — callers are
		// expected to check that driver names/fields are non-empty if they
		// care.
		s_fetch_callback.call(info);
	}
}

// --- /api/v1/drivers callback ---------------------------------------------

void IndustrialRuntimeClient::_on_drivers_completed(
		int p_result, int p_response_code,
		const PackedStringArray &p_headers,
		const PackedByteArray &p_body) {
	(void)p_headers;

	bool ok = true;
	String err;
	if (p_result != OK) {
		err = vformat("transport %d", p_result);
		ok = false;
	} else if (p_response_code != 200) {
		err = vformat("HTTP %d", p_response_code);
		ok = false;
	}
	Dictionary dict;
	if (ok) {
		dict = _parse_response_body(p_body, &err);
		if (dict.is_empty()) {
			ok = false;
		}
	}
	if (!ok) {
		print_line(vformat("industrial_runtime: drivers request failed (%s), leaving empty (fallback via industrial_driver_schema.cpp)", err));
		s_driver_catalog_ready = true;
		_maybe_settle();
		return;
	}

	Array list = dict.get("drivers", Array());
	// Store exactly as returned — the form layer reads via get_driver_fields,
	// get_driver_name, get_driver_count.
	s_driver_catalog = list;
	s_driver_catalog_ready = true;
	print_line(vformat("industrial_runtime: loaded %d drivers from /drivers", list.size()));

	_maybe_settle();
}

// --- /api/v1/drivers/s7/address-catalog callback -------------------------

void IndustrialRuntimeClient::_on_s7_addr_completed(
		int p_result, int p_response_code,
		const PackedStringArray &p_headers,
		const PackedByteArray &p_body) {
	(void)p_headers;

	bool ok = true;
	String err;
	if (p_result != OK) {
		err = vformat("transport %d", p_result);
		ok = false;
	} else if (p_response_code != 200) {
		err = vformat("HTTP %d", p_response_code);
		ok = false;
	}
	Dictionary dict;
	if (ok) {
		dict = _parse_response_body(p_body, &err);
		if (dict.is_empty()) {
			ok = false;
		}
	}
	if (!ok) {
		print_line(vformat("industrial_runtime: S7 addr failed (%s), using builtin fallback", err));
		s_s7_address_types = s7_address_types_fallback();
		s_address_catalog_ready = true;
		_maybe_settle();
		return;
	}

	s_s7_address_catalog = dict.get("address_types", Array());
	Array types = s_s7_address_catalog;
	PackedStringArray names;
	names.resize(0);
	for (int i = 0; i < types.size(); i++) {
		Dictionary entry = types[i];
		String name = entry.get("name", "");
		if (name.length() > 0) {
			names.append(name);
		}
	}
	if (names.is_empty()) {
		print_line("industrial_runtime: server returned empty S7 catalog; fallback");
		names = s7_address_types_fallback();
	}

	s_s7_address_types = names;
	s_address_catalog_ready = true;
	print_line(vformat("industrial_runtime: loaded %d S7 address types", names.size()));
	_maybe_settle();
}

// ============================================================================
// Generic per-driver tag-field catalog (NEW 2026-08-22: 地址模式/地址类型/资料格式联动)
// ============================================================================

// Static member definitions for the new cache.
Dictionary IndustrialRuntimeClient::s_tagfield_cache;
Dictionary IndustrialRuntimeClient::s_tagfield_http;
Dictionary IndustrialRuntimeClient::s_tagfield_callbacks;

// ---------------------------------------------------------------------------
// Common fallback helpers
// ---------------------------------------------------------------------------
namespace {

Array _common_address_modes_ui() {
	Array a;
	Dictionary w; w["id"] = "word"; w["label_msgid"] = "Word"; w["label_zh"] = String::utf8("字"); a.push_back(w);
	Dictionary b; b["id"] = "bit";  b["label_msgid"] = "Bit";  b["label_zh"] = String::utf8("位"); a.push_back(b);
	return a;
}

Array _common_data_formats() {
	Array r;
	struct Entry { const char *id; const char *msgid; const char *zh; int bv14; int bw; const char *cat; const char *pr; };
	const Entry T[] = {
		{"i16",    "Signed Integer (16-bit)",   "有符号整数 (16-bit)",  0x00, 2, "integer", "@H"},
		{"u16",    "Unsigned Integer (16-bit)", "无符号整数 (16-bit)",  0x00, 2, "integer", "@E"},
		{"i32",    "Signed Integer (32-bit)",   "有符号长整数 (32-bit)",0x00, 4, "integer", "@I"},
		{"u32",    "Unsigned Integer (32-bit)", "无符号长整数 (32-bit)",0x00, 4, "integer", "@G"},
		{"u8",     "Byte",                      "字节",                0x00, 1, "integer", "@C"},
		{"f32",    "Float (32-bit)",            "浮点数 (32-bit)",      0x20, 4, "float",   "@F"},
		{"f64",    "Double (64-bit)",           "双精度浮点数 (64-bit)",0x20, 8, "float",   "@B"},
		{"bcd16",  "BCD (16-bit)",              "BCD 码 (16-bit)",      0x08, 2, "bcd",     "bcd16_"},
		{"bcd32",  "BCD (32-bit)",              "BCD 码 (32-bit)",      0x08, 4, "bcd",     "bcd32_"},
		{"real32", "Real32 (IEEE 754)",         "实数 32 位 (IEEE 754)",0x20, 4, "float",   "real32_"},
		{"str_a",  "String (ASCII)",            "字符串 (ASCII)",       0x00, 0, "string",  "@S"},
		{"str_w",  "Wide String (UTF-16)",      "宽字符串 (UTF-16)",    0x00, 0, "string",  "@W"},
	};
	for (const auto &e : T) {
		Dictionary d;
		d["id"] = String(e.id);
		d["label_msgid"] = String(e.msgid);
		d["label_zh"] = String::utf8(e.zh);
		d["bvar14_hex"] = e.bv14;
		d["byte_width"] = e.bw;
		d["category"] = String(e.cat);
		d["ebpro_prefix"] = String(e.pr);
		r.push_back(d);
	}
	return r;
}

Dictionary _mk_at(const char *id, const char *label, const char *zh, bool bit, int width,
		const char *df, const char *grp, bool reqDB = false, bool hasLen = false,
		int internalCode = 0, int areaByte = 0) {
	Dictionary d;
	d["id"] = String(id);
	d["label"] = String(label);
	d["label_zh"] = String::utf8(zh);
	d["bit_addressable"] = bit;
	d["word_width"] = width;
	d["default_data_format"] = String(df);
	d["area_group"] = String(grp);
	d["requires_db"] = reqDB;
	d["has_length"] = hasLen;
	d["internal_type_code"] = internalCode;
	d["area_byte"] = areaByte;
	return d;
}

Array _fallback_s7_addrtypes() {
	Array r;
	// Inputs
	r.push_back(_mk_at("IB","IB","输入字节",true,1,"u8","Input",false,false,0,0x68));
	r.push_back(_mk_at("IW","IW","输入字",  true,2,"u16","Input",false,false,0,0x73));
	r.push_back(_mk_at("ID","ID","输入双字",true,4,"u32","Input",false,false,0,0x74));
	// Outputs
	r.push_back(_mk_at("QB","QB","输出字节",true,1,"u8","Output",false,false,0,0x6D));
	r.push_back(_mk_at("QW","QW","输出字",  true,2,"u16","Output",false,false,0,0x75));
	r.push_back(_mk_at("QD","QD","输出双字",true,4,"u32","Output",false,false,0,0x76));
	// Merker
	r.push_back(_mk_at("MB","MB","标志字节",true,1,"u8","Marker",false,false,0,0x72));
	r.push_back(_mk_at("MW","MW","标志字",  true,2,"u16","Marker",false,false,0,0x70));
	r.push_back(_mk_at("MD","MD","标志双字",true,4,"u32","Marker",false,false,0,0x71));
	// DB
	r.push_back(_mk_at("DBBn","DBBn","DB 字节 (DB号)", true,1,"u8","DB",true));
	r.push_back(_mk_at("DBBx","DBBx","DB 字节 (优化)", true,1,"u8","DB",false));
	r.push_back(_mk_at("DBn","DBn","DB 字 (DB号)",   true,2,"u16","DB",true));
	r.push_back(_mk_at("DBx","DBx","DB 字 (优化)",   true,2,"u16","DB",false));
	r.push_back(_mk_at("DBDn","DBDn","DB 双字 (DB号)", true,4,"u32","DB",true));
	r.push_back(_mk_at("DBDx","DBDx","DB 双字 (优化)", true,4,"u32","DB",false));
	// DB strings
	r.push_back(_mk_at("DBn_String","DBn_String","DB ASCII字符串 (DB号字)", true,0,"str_a","DB_String",true,true));
	r.push_back(_mk_at("DBx_String","DBx_String","DB ASCII字符串 (优化字)", true,0,"str_a","DB_String",false,true));
	r.push_back(_mk_at("DBn_String1","DBn_String1","DB ASCII字符串1 (DB号字节)", true,0,"str_a","DB_String",true,true));
	r.push_back(_mk_at("DBx_String1","DBx_String1","DB ASCII字符串1 (优化字节)", true,0,"str_a","DB_String",false,true));
	r.push_back(_mk_at("DBn_WString","DBn_WString","DB UTF-16宽字符串 (DB号)", true,0,"str_w","DB_WString",true,true));
	r.push_back(_mk_at("DBx_WString","DBx_WString","DB UTF-16宽字符串 (优化)", true,0,"str_w","DB_WString",false,true));
	r.push_back(_mk_at("DBDn_String","DBDn_String","DB ASCII字符串 (DB号双字起始)", true,0,"str_a","DB_String",true,true));
	r.push_back(_mk_at("DBDx_String","DBDx_String","DB ASCII字符串 (优化双字起始)", true,0,"str_a","DB_String",false,true));
	// S5TIME
	r.push_back(_mk_at("S5TIME_10Ms", "S5TIME 10ms", "S5 定时器 (10ms)", false,2,"u16","S5TIME",false,false,0,0xF7));
	r.push_back(_mk_at("S5TIME_100Ms","S5TIME 100ms","S5 定时器 (100ms)",false,2,"u16","S5TIME",false,false,0,0xF8));
	r.push_back(_mk_at("S5TIME_1S",   "S5TIME 1s",   "S5 定时器 (1秒)",  false,2,"u16","S5TIME",false,false,0,0xF9));
	r.push_back(_mk_at("S5TIME_10S",  "S5TIME 10s",  "S5 定时器 (10秒)", false,2,"u16","S5TIME",false,false,0,0xFA));
	return r;
}

Array _fallback_modbus_addrtypes() {
	Array r;
	r.push_back(_mk_at("0x_Coil_Bit",      "0x_Coil (位)",     "线圈 (0x区,位)",        true, 1,"bit",  "Coil"));
	r.push_back(_mk_at("0x_Coil_Word",     "0x_Coil",          "线圈打包字 (16个线圈)", true, 2,"u16",  "Coil"));
	r.push_back(_mk_at("1x_InputBit",      "1x_InputBit (位)", "离散输入 (1x区,位)",    true, 1,"bit",  "InputBit"));
	r.push_back(_mk_at("1x_InputBit_Word", "1x_InputBit",      "离散输入 打包字",        true, 2,"u16",  "InputBit"));
	r.push_back(_mk_at("3x_InputReg",      "3x_InputReg",      "输入寄存器 (3x 只读)",  true, 2,"u16",  "InputReg"));
	r.push_back(_mk_at("3x_InputReg32",    "3x_InputReg_32",   "输入寄存器 32 位 (双字)",false,4,"u32",  "InputReg"));
	r.push_back(_mk_at("3x_InputRegF",     "3x_InputReg_Float","输入寄存器浮点 (双寄存器)",false,4,"f32","InputReg"));
	r.push_back(_mk_at("4x_HR",            "4x_HR",            "保持寄存器 (4x 可读写)", true, 2,"u16",  "HoldingReg"));
	r.push_back(_mk_at("4x_HR_32",         "4x_HR_32",         "保持寄存器 32 位",       false,4,"u32",  "HoldingReg"));
	r.push_back(_mk_at("4x_HR_Float",      "4x_HR_Float",      "保持寄存器浮点",         false,4,"f32",  "HoldingReg"));
	r.push_back(_mk_at("4x_HR_String",     "4x_HR_String",     "保持寄存器 ASCII 字符串",false,0,"str_a","HoldingReg",false,true));
	r.push_back(_mk_at("4x_Bit",           "4x_Bit",           "保持寄存器位 (x.0-x.15)", true, 1,"bit",  "HoldingReg"));
	return r;
}

Array _fallback_slmp_addrtypes() {
	Array r;
	struct E { const char* id, *lb, *zh; bool bit; int w; const char *df; const char *grp; };
	const E T[] = {
		{"X_Bit","X (位)","输入继电器X(位)",true,1,"bit","Input"},
		{"X_Word","X","输入继电器X(字)", true,2,"u16","Input"},
		{"Y_Bit","Y (位)","输出继电器Y(位)",true,1,"bit","Output"},
		{"Y_Word","Y","输出继电器Y(字)", true,2,"u16","Output"},
		{"M_Bit","M (位)","内部继电器M(位)",true,1,"bit","Internal"},
		{"M_Word","M","内部继电器M(字)", true,2,"u16","Internal"},
		{"L_Word","L","锁存继电器L",       true,2,"u16","Latch"},
		{"F_Word","F","报警继电器F",       true,2,"u16","Internal"},
		{"B_Word","B","链接继电器B",       true,2,"u16","Link"},
		{"W","W","链接寄存器W",           true,2,"u16","Link"},
		{"D","D","数据寄存器D (16-bit)",  true,2,"u16","Data"},
		{"D_32","D32","数据寄存器D (32-bit)",false,4,"u32","Data"},
		{"D_Float","DF","数据寄存器D Float32",false,4,"f32","Data"},
		{"R","R","文件寄存器R (16-bit)",  true,2,"u16","File"},
		{"ZR","ZR","扩展数据寄存器ZR(Q)", true,2,"u16","Data"},
		{"TS","TS","定时器触点TS(位)",    false,1,"bit","Timer"},
		{"TC","TC","定时器线圈TC(位)",    false,1,"bit","Timer"},
		{"TN","TN","定时器当前值TN",      true,2,"u16","Timer"},
		{"CS","CS","计数器触点CS(位)",    false,1,"bit","Counter"},
		{"CC","CC","计数器线圈CC(位)",    false,1,"bit","Counter"},
		{"CN","CN","计数器当前值CN",      true,2,"u16","Counter"},
		{"SD","SD","特殊寄存器SD",        true,2,"u16","Special"},
		{"SM_Bit","SM (位)","特殊继电器SM(位)",false,1,"bit","Special"},
		{"SM_Word","SM","特殊继电器SM(字)",true,2,"u16","Special"},
	};
	for (const auto &e : T) r.push_back(_mk_at(e.id,e.lb,e.zh,e.bit,e.w,e.df,e.grp));
	return r;
}

Array _fallback_fins_addrtypes() {
	Array r;
	struct E { const char* id, *lb, *zh; bool bit; int w; const char *df; const char *grp; };
	const E T[] = {
		{"CIO_Bit","CIO 位","CIO区位 (输入/输出/槽)",true,1,"bit","CIO"},
		{"CIO_Word","CIO","CIO区字 (16-bit)",true,2,"u16","CIO"},
		{"WR_Bit","WR 位","工作继电器WR位",true,1,"bit","WR"},
		{"WR_Word","WR","工作继电器WR字",true,2,"u16","WR"},
		{"HR_Bit","HR 位","保持继电器HR位",true,1,"bit","HR"},
		{"HR_Word","HR","保持继电器HR字",true,2,"u16","HR"},
		{"AR_Bit","AR 位","辅助继电器AR位",true,1,"bit","AR"},
		{"AR_Word","AR","辅助继电器AR字",true,2,"u16","AR"},
		{"DM","DM","数据存储器DM (16-bit)",true,2,"u16","DM"},
		{"DM_32","DM32","数据存储器DM (32-bit)",false,4,"u32","DM"},
		{"DM_Float","DMF","DM浮点 (IEEE754)",false,4,"f32","DM"},
		{"EM0","EM bank 0","扩展存储器Bank 0",true,2,"u16","EM"},
		{"EM1","EM bank 1","扩展存储器Bank 1",true,2,"u16","EM"},
		{"EM2","EM bank 2","扩展存储器Bank 2",true,2,"u16","EM"},
		{"EM3","EM bank 3","扩展存储器Bank 3",true,2,"u16","EM"},
		{"TIM","TIM","定时器完成标志TIM(位)",false,1,"bit","Timer"},
		{"TIM_PV","TIM PV","定时器当前值BCD",false,2,"bcd16","Timer"},
		{"CNT","CNT","计数器完成标志CNT(位)",false,1,"bit","Counter"},
		{"CNT_PV","CNT PV","计数器当前值BCD",false,2,"bcd16","Counter"},
		{"TK_Bit","TK (位)","任务标志TK(位)",false,1,"bit","Task"},
		{"TK_Word","TK","任务标志TK(字)",true,2,"u16","Task"},
		{"IR_Word","IR","变址寄存器IR(32-bit)",false,4,"u32","Index"},
		{"DR_Word","DR","数据寄存器DR(32-bit)",false,4,"u32","Index"},
		{"SR_Word","SR","状态寄存器SR(16-bit)",true,2,"u16","Status"},
	};
	for (const auto &e : T) r.push_back(_mk_at(e.id,e.lb,e.zh,e.bit,e.w,e.df,e.grp));
	return r;
}

Dictionary _make_absolute(const String &key, const Array &at) {
	Dictionary r;
	r["driver_key"]      = key;
	r["addressing_mode"] = "absolute";
	r["address_modes_ui"]= _common_address_modes_ui();
	r["address_types"]   = at;
	r["data_formats"]    = _common_data_formats();
	r["symbolic_import"] = Dictionary();
	return r;
}

Dictionary _make_symbolic(const String &key, const Array &exts, const Array &hints) {
	Dictionary r;
	r["driver_key"]      = key;
	r["addressing_mode"] = "symbolic";
	r["address_modes_ui"]= _common_address_modes_ui();
	r["address_types"]   = Array();
	r["data_formats"]    = _common_data_formats();
	Dictionary s;
	s["supported"]         = true;
	s["project_file_exts"] = exts;
	s["tag_name_hints"]    = hints;
	r["symbolic_import"] = s;
	return r;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

Dictionary IndustrialRuntimeClient::_tag_field_fallback(const String &p_driver_key) {
	// ---- symbolic drivers ----
	struct Sym {
		const char *k;
		const char *exts[5];   // terminated by nullptr
		const char *hints[5];  // terminated by nullptr
	};
	const Sym syms[] = {
		{"mitsubishi_qlr_symbolic",
		 {".GLOBAL_EXPORT",".csv",nullptr},
		 {"Module1.Temperature","GX Works3 全局标签名",nullptr}},
		{"omron_nx_nj_eip_tag",
		 {".VAR",".csv",nullptr},
		 {"Global_Variable_Name","NX/NJ 变量名",nullptr}},
		{"schneider_m580_tag",
		 {".XVM",".XSY",nullptr},
		 {"DFB.Block.Input","Unity Pro 顶层变量 / DFB 标签名",nullptr}},
		{"ge_pac_rx3i_eip_tag",
		 {".PACSYM",".csv",nullptr},
		 {"ControllerTags.FaultWord","PACSystems RX3i 符号名",nullptr}},
		{"ab_controllogix_eip_class3_tag",
		 {".L5X",".CSV",nullptr},
		 {"Program:MainProgram.Counter.ACC","Logix 5000 符号路径",nullptr}},
		{"beckhoff_cx_embedded_ads",
		 {".tmcproj",nullptr},
		 {"MAIN.bVar1","GVL.nCounter",nullptr}},
		{"beckhoff_twincat3_tag",
		 {".tmcproj",".tcl",".tszip",nullptr},
		 {"GVL.Temperature","导入 .tmcproj 项目后的全局符号",nullptr}},
		{"hollysys_symbolic_ethernet",
		 {".hdb",".h5",".csv",nullptr},
		 {"GLOBAL.AI_001","HollySys 项目符号名",nullptr}},
		{"novastar_symbolic_ethernet",
		 {".ldproj",nullptr},
		 {"Screen.Power","Nova LED 控制卡 300+ 专用标签",nullptr}},
		{"j1939_can_symbolic",
		 {".dbc",".j1939.csv",nullptr},
		 {"SPN.899 (Engine Speed)","PGN:SPN:Bit:BitLen 格式",nullptr}},
	};
	for (const auto &s : syms) {
		if (p_driver_key == String(s.k)) {
			Array ex, hi;
			for (int i = 0; s.exts[i];  i++) ex.push_back(String(s.exts[i]));
			for (int i = 0; s.hints[i]; i++) hi.push_back(String::utf8(s.hints[i]));
			return _make_symbolic(s.k, ex, hi);
		}
	}

	// ---- absolute S7 family ----
	if (p_driver_key.begins_with("siemens_s7_") ||
		p_driver_key.begins_with("s7_")) {
		return _make_absolute(p_driver_key, _fallback_s7_addrtypes());
	}
	// ---- Mitsubishi absolute SLMP / FX / MC ----
	{
		static const char *mits_abs_prefixes[] = {
			"mitsubishi_fx_", "mitsubishi_fx3u", "mitsubishi_fx5u",
			"mitsubishi_qlr_slmp_", "mitsubishi_q_mc_", "mitsubishi_qna_mc_",
			nullptr,
		};
		bool hit = false;
		for (int i = 0; mits_abs_prefixes[i]; i++) {
			if (p_driver_key.begins_with(mits_abs_prefixes[i])) { hit = true; break; }
		}
		if (hit) {
			return _make_absolute(p_driver_key, _fallback_slmp_addrtypes());
		}
	}
	// ---- Omron FINS absolute ----
	if (p_driver_key == "omron_cs_cj_cp_fins_ethernet" ||
		p_driver_key == "omron_cv_fins_ethernet" ||
		p_driver_key == "omron_hostlink" ||
		p_driver_key == "omron_cv_toolbus") {
		return _make_absolute(p_driver_key, _fallback_fins_addrtypes());
	}
	// ---- default: Modbus-like fallback for everything else (41 Modbus +
	//      vendor Modbus + unknown driver keys).  Never return an empty table
	//      or the user thinks the dialog is broken.
	return _make_absolute(p_driver_key, _fallback_modbus_addrtypes());
}

void IndustrialRuntimeClient::fetch_tag_field_catalog(const String &p_driver_key,
		Node *p_owner, Callable p_callback) {
	ERR_FAIL_NULL(p_owner);
	if (p_driver_key.is_empty()) {
		if (p_callback.is_valid()) {
			Array a; a.push_back(false);
			p_callback.call(a);
		}
		return;
	}
	// If already cached: invoke callback synchronously (fast path).
	if (s_tagfield_cache.has(p_driver_key)) {
		if (p_callback.is_valid()) {
			Array a; a.push_back(true);
			p_callback.call(a);
		}
		return;
	}
	// If a request is already in flight for this driver_key, just register
	// the callback (last-registered wins; we don't need a fan-out vector).
	HTTPRequest *existing = nullptr;
	if (s_tagfield_http.has(p_driver_key)) {
		// stored as Object ID via Variant; HTTPRequest lookup via cast from intptr
		ObjectID oid = (ObjectID)(uint64_t)s_tagfield_http[p_driver_key];
		existing = Object::cast_to<HTTPRequest>(ObjectDB::get_instance(oid));
	}
	String base_url = get_runtime_url();

	auto *req = existing;
	if (!req) {
		req = memnew(HTTPRequest);
		req->set_timeout(10.0);
		p_owner->add_child(req);
		// Bind with a closure-like lambda via callable_mp_static and bind the
		// driver key as an argument.  Godot's Callable::bind returns a new
		// Callable that prepends the bound arguments on invocation.
		Callable c = callable_mp_static(&IndustrialRuntimeClient::_on_tagfield_completed);
		c = c.bind(p_driver_key);
		req->connect("request_completed", c);
		s_tagfield_http[p_driver_key] = Variant((uint64_t)req->get_instance_id());
	}
	// Always update the registered callback so the latest caller gets notified
	// (overwrite any previous caller's callback — simpler than fanning out).
	if (p_callback.is_valid()) {
		s_tagfield_callbacks[p_driver_key] = p_callback;
	}

	String url = base_url + "/api/v1/drivers/" + p_driver_key.uri_encode() + "/tag-field-catalog";
	Error err = req->request(url);
	if (err != OK) {
		print_line(vformat("industrial_runtime: tagfield request failed (%d) for driver=%s — fallback", err, p_driver_key));
		s_tagfield_cache[p_driver_key] = _tag_field_fallback(p_driver_key);
		// Fire callback (via stored callable) if set.
		if (s_tagfield_callbacks.has(p_driver_key)) {
			Callable cb = s_tagfield_callbacks[p_driver_key];
			Array a; a.push_back(false);
			cb.call(a);
		}
	} else {
		print_line(vformat("industrial_runtime: GET %s", url));
	}
}

Dictionary IndustrialRuntimeClient::get_tag_field_catalog(const String &p_driver_key) {
	if (p_driver_key.is_empty()) {
		return _tag_field_fallback(p_driver_key);
	}
	if (s_tagfield_cache.has(p_driver_key)) {
		return Dictionary(s_tagfield_cache[p_driver_key]);
	}
	// No network request yet → return fallback result synchronously.  When the
	// async fetch completes later it populates the cache and fires the stored
	// callback so the calling dialog can refill itself.
	return _tag_field_fallback(p_driver_key);
}

PackedStringArray IndustrialRuntimeClient::get_address_type_labels(const String &p_driver_key) {
	Dictionary d = get_tag_field_catalog(p_driver_key);
	Array arr = d.get("address_types", Array());
	PackedStringArray out;
	out.resize(0);
	for (int i = 0; i < arr.size(); i++) {
		Dictionary at = arr[i];
		String label = at.get("label", Variant(""));
		if (label.length() == 0) {
			label = at.get("id", Variant(""));
		}
		String zh = at.get("label_zh", Variant(""));
		// Prefer "Label (中文)" in the dropdown for quick scanning.  Keep the
		// raw `id` in the metadata for round-tripping with the backend (the
		// dialog stores OptionButton metadata so callers get AT id via
		// get_metadata).
		String display = label;
		if (zh.length() > 0) {
			display = label + "  (" + zh + ")";
		}
		out.append(display);
	}
	return out;
}

PackedStringArray IndustrialRuntimeClient::get_address_type_ids(const String &p_driver_key) {
	Dictionary d = get_tag_field_catalog(p_driver_key);
	Array arr = d.get("address_types", Array());
	PackedStringArray out;
	out.resize(0);
	for (int i = 0; i < arr.size(); i++) {
		Dictionary at = arr[i];
		out.append(at.get("id", Variant("")));
	}
	return out;
}

PackedStringArray IndustrialRuntimeClient::get_data_format_msgids(const String &p_driver_key) {
	Dictionary d = get_tag_field_catalog(p_driver_key);
	Array arr = d.get("data_formats", Array());
	PackedStringArray out;
	out.resize(0);
	for (int i = 0; i < arr.size(); i++) {
		Dictionary df = arr[i];
		out.append(df.get("label_msgid", Variant("")));
	}
	return out;
}

// ---------------------------------------------------------------------------
// HTTP callback
// ---------------------------------------------------------------------------
void IndustrialRuntimeClient::_on_tagfield_completed(int p_result, int p_response_code,
		const PackedStringArray &p_headers, const PackedByteArray &p_body,
		const String &p_driver_key) {
	(void)p_headers;

	bool ok = true;
	String err;
	if (p_result != OK) {
		err = vformat("transport %d", p_result);
		ok = false;
	} else if (p_response_code != 200) {
		err = vformat("HTTP %d", p_response_code);
		ok = false;
	}
	Dictionary dict;
	if (ok) {
		dict = _parse_response_body(p_body, &err);
		if (dict.is_empty()) ok = false;
	}
	if (!ok) {
		print_line(vformat("industrial_runtime: tagfield driver=%s failed (%s), fallback hardcoded", p_driver_key, err));
		Dictionary fallback = _tag_field_fallback(p_driver_key);
		s_tagfield_cache[p_driver_key] = fallback;
	} else {
		s_tagfield_cache[p_driver_key] = dict;
		print_line(vformat("industrial_runtime: tagfield driver=%s loaded AT=%d DF=%d",
			p_driver_key,
			Array(dict.get("address_types", Array())).size(),
			Array(dict.get("data_formats",   Array())).size()));
	}
	// Free the HTTPRequest: safe because the callback has fired.
	if (s_tagfield_http.has(p_driver_key)) {
		ObjectID oid = (ObjectID)(uint64_t)s_tagfield_http[p_driver_key];
		HTTPRequest *r = Object::cast_to<HTTPRequest>(ObjectDB::get_instance(oid));
		if (r && r->get_parent()) {
			r->queue_free();
		}
		s_tagfield_http.erase(p_driver_key);
	}
	// Fire stored callback (if any) with a single boolean argument: true=ok
	if (s_tagfield_callbacks.has(p_driver_key)) {
		Callable cb = s_tagfield_callbacks[p_driver_key];
		s_tagfield_callbacks.erase(p_driver_key);
		Array a;
		a.push_back(ok);
		cb.call(a);
	}
}

// --- Project creation API (Superpowers workflow entry) ------------------------

void IndustrialRuntimeClient::create_project(const String &p_project_id, const String &p_project_name, Node *p_owner,
		const Callable &p_on_success) {
	s_create_project_success_callback = p_on_success;

	String base_url = get_runtime_url();
	HTTPRequest *req = memnew(HTTPRequest);
	req->set_timeout(10.0);
	p_owner->add_child(req);
	req->connect("request_completed", callable_mp_static(&IndustrialRuntimeClient::_on_create_project_completed));

	String url = base_url + "/api/v1/project";
	Dictionary payload;
	payload["project_id"] = p_project_id;
	payload["project_name"] = p_project_name;
	String body = JSON::stringify(payload);

	print_line(vformat("industrial_runtime: POST %s body=%s", url, body));
	Error err = req->request(url, PackedStringArray(), HTTPClient::METHOD_POST, body);
	if (err != OK) {
		print_line(vformat("industrial_runtime: create_project request failed (%d)", err));
		s_create_project_success_callback = Callable();
	}
}

void IndustrialRuntimeClient::_on_create_project_completed(int p_result, int p_response_code,
		const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	(void)p_headers;
	Callable on_success = s_create_project_success_callback;
	s_create_project_success_callback = Callable();

	if (p_result != OK) {
		print_line(vformat("industrial_runtime: create_project failed (result=%d)", p_result));
		return;
	}
	if (p_response_code != 200) {
		String body = String::utf8((const char *)p_body.ptr(), p_body.size());
		print_line(vformat("industrial_runtime: create_project HTTP %d body=%s", p_response_code, body));
		return;
	}
	String body = String::utf8((const char *)p_body.ptr(), p_body.size());
	print_line(vformat("industrial_runtime: create_project OK: %s", body));

	if (on_success.is_valid()) {
		on_success.call();
	}
}

void IndustrialRuntimeClient::import_project(const String &p_json_body, Node *p_owner) {
	String base_url = get_runtime_url();
	HTTPRequest *req = memnew(HTTPRequest);
	req->set_timeout(30.0);
	p_owner->add_child(req);
	req->connect("request_completed", callable_mp_static(&IndustrialRuntimeClient::_on_import_project_completed));

	String url = base_url + "/api/v1/project/import";
	print_line(vformat("industrial_runtime: POST %s (nested import, %d bytes)", url, p_json_body.length()));
	Error err = req->request(url, PackedStringArray(), HTTPClient::METHOD_POST, p_json_body);
	if (err != OK) {
		print_line(vformat("industrial_runtime: import_project request failed (%d)", err));
	}
}

void IndustrialRuntimeClient::_on_import_project_completed(int p_result, int p_response_code,
		const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	(void)p_headers;
	if (p_result != OK) {
		print_line(vformat("industrial_runtime: import_project failed (result=%d)", p_result));
		return;
	}
	if (p_response_code != 200) {
		String body = String::utf8((const char *)p_body.ptr(), p_body.size());
		print_line(vformat("industrial_runtime: import_project HTTP %d body=%s", p_response_code, body));
		return;
	}
	String body = String::utf8((const char *)p_body.ptr(), p_body.size());
	print_line(vformat("industrial_runtime: import_project OK: %s", body));
}
