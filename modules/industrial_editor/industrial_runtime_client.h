#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class HTTPRequest;
class Node;

// A single driver-defined field descriptor (mirror of industrial_driver_schema.h,
// kept with a forward declaration here to avoid a tight include cycle).
// The exact-layout guarantee comes from the struct declaration below.
struct IndustrialFieldDef;

/// Cross-platform HTTP client for fetching runtime metadata from the Go backend.
///
/// The editor UI (drivers, address types, device catalogs) must match what
/// the runtime actually supports.  This client pulls that data at editor
/// startup via the HTTP API defined in internal/api/http.go.
///
/// Design notes:
/// - Uses Godot's built-in HTTPRequest node (works on every platform, no Win32).
/// - Caches results in static containers so dialogs can call
///   `get_s7_address_types()` synchronously after the initial load completes.
/// - Falls back to a hard-coded list when the runtime is unreachable.
///
/// Follows the same pattern as EngineUpdateLabel in project_manager/:
///   scene/main/http_request.h  →  HTTPRequest node
///   core/io/json.h            →  JSON::parse_string
///   core/string/ustring.h     →  String::utf8
///   PackedStringArray/Array   →  Godot container types (no std::vector/map)
class IndustrialRuntimeClient : public RefCounted {
	GDCLASS(IndustrialRuntimeClient, RefCounted);

public:
	/// URL of the Go runtime HTTP API.
	///
	/// Resolved from (highest-priority wins; all optional):
	///   1. Cmdline arg:     --runtime-url=<url>
	///   2. Env var:         GD_RUNTIME_URL
	///   3. ProjectSettings: industrial/runtime/url   (随 project.godot 导出 → 可用在打包后的程序)
	///   4. EditorSettings:  industrial/runtime/url   (仅编辑器态，存用户全局偏好)
	///   5. Fallback:        http://127.0.0.1:8080
	///
	/// The URL should NOT include a trailing slash.
	static String get_runtime_url();

	/// Public constant so settings dialogs, docs and code can reuse the
	/// exact setting keys used by layers 3 and 4 above.
	static constexpr const char *kProjectSettingKey = "industrial/runtime/url";
	static constexpr const char *kEditorSettingKey  = "industrial/runtime/url";
	static constexpr const char *kCmdlineFlag       = "--runtime-url=";
	static constexpr const char *kEnvVar            = "GD_RUNTIME_URL";
	static constexpr const char *kDefaultUrl        = "http://127.0.0.1:8080";

	/// Kick off an async fetch of all metadata.
	/// The HTTPRequest node is attached to p_owner so it lives for the
	/// duration of the request (matches EngineUpdateLabel pattern).
	static void fetch_metadata(Node *p_owner);

	// ---- S7 address type catalog (cached after fetch) ----
	/// Returns true if the address catalog has been loaded (or fell back).
	static bool is_address_catalog_ready();

	/// Returns the S7 address type names for the UI picker.
	/// If the HTTP fetch hasn't completed yet, falls back to a hard-coded list.
	static PackedStringArray get_s7_address_types();

	/// Returns the full address catalog entries as an array of dictionaries.
	/// Each dict has keys: name, area, value_kind, size, etc.
	static Array get_s7_address_catalog();

	// ---- Driver catalog ----
	/// Full catalog returned by GET /api/v1/drivers.
	/// Each entry is a Dictionary with keys: index, display_name, driver_key,
	/// and fields[] (Array of Dictionary: label_msgid, key, kind,
	/// default_value, min_value, max_value, choice_labels[], tooltip_msgid).
	static Array get_driver_catalog();

	/// Number of drivers loaded (or fallback count).  Mirrors the enumeration
	/// used by industrial_driver_schema.cpp (legacy 0..N indexing is preserved
	/// so other modules can call both paths without confusion).
	static int get_driver_count();

	/// Returns the driver display name for the legacy int enumeration
	/// `p_driver` (0-based).  If nothing was cached, returns "" and the caller
	/// should fall back to hardcoded values.
	static String get_driver_name(int p_driver);

	/// Returns the runtime driver key (e.g. "siemens_s7") for enumeration
	/// `p_driver`.  Used to save/load project files so the driver identity
	/// survives enumeration reordering.
	static String get_driver_key(int p_driver);

	/// Driver metadata accessors (read directly from cached Dictionary;
	/// empty/false when the runtime catalog isn't loaded).
	static String get_driver_vendor(int p_driver);           // category_vendor
	static String get_driver_interface(int p_driver);         // category_interface
	static String get_driver_addressing_mode(int p_driver);   // absolute | symbolic
	static bool   get_driver_supports_tag_import(int p_driver);
	static bool   get_driver_supports_udp(int p_driver);
	static bool   get_driver_supports_perf_params(int p_driver);

	/// Returns the connection-parameter schema fields for driver enumeration
	/// `p_driver` (Vector<IndustrialFieldDef>).  If the API result is not
	/// ready the returned vector is empty, and callers MUST fall back to the
	/// local hardcoded schema.
	static Vector<struct IndustrialFieldDef> get_driver_fields(int p_driver);

	/// Signal emitted when the metadata fetch completes (success or failure).
	/// Consumers can connect to this to refresh their UI.
	static void set_fetch_callback(Callable p_callback);

	// ---- Generic per-driver tag-field catalog (NEW in 2026-08-22) ----
	// GET /api/v1/drivers/:driver_key/tag-field-catalog
	// Returns Address Modes UI × Address Types list × Data Formats list for
	// the specified driver.  Symbolic drivers return an empty address_types
	// array and populate symbolic_import.  Results are cached per driver key
	// so repeated dialog opens never hit the network twice.  If the HTTP
	// fetch has not completed yet or the backend is unreachable, falls back
	// to a family-specific hard-coded list (never returns a blank dropdown).

	/// Initiates an async fetch for the specified driver.  p_owner must be a
	/// valid live Node (the HTTPRequest is attached to it).  When the request
	/// completes, p_callback is called once with a single boolean argument
	/// (true=success).  Callers that don't care about completion can pass an
	/// empty Callable; get_tag_field_catalog_dict() simply returns whatever
	/// cached/fallback result is ready.
	static void fetch_tag_field_catalog(const String &p_driver_key, Node *p_owner, Callable p_callback = Callable());

	/// Sends a POST /api/v1/project to the runtime with only project_id
	/// and project_name. Used by the Superpowers workflow to register a project.
	/// The HTTPRequest is attached to p_owner. Logs the result to console.
	/// When p_on_success is valid it is invoked once after HTTP 200.
	static void create_project(const String &p_project_id, const String &p_project_name, Node *p_owner,
			const Callable &p_on_success = Callable());

	/// Posts nested project JSON to POST /api/v1/project/import (apply defaults
	/// true on the backend when omitted). Body is the full import payload
	/// (devices with tags, optional project_id/name, optional apply).
	static void import_project(const String &p_json_body, Node *p_owner);

	/// Returns the cached/fallback catalog Dictionary for p_driver_key.
	/// Top-level keys mirror the backend JSON:
	///   driver_key (String) / addressing_mode (String "absolute"|"symbolic") /
	///   address_modes_ui (Array of Dict{id,label_msgid,label_zh}) /
	///   address_types (Array of Dict{id,label,label_zh,bit_addressable,
	///                    word_width,default_data_format,area_group,
	///                    requires_db,has_length,internal_type_code,area_byte}) /
	///   data_formats (Array of Dict{id,label_msgid,label_zh,bvar14_hex,
	///                     byte_width,category,ebpro_prefix}) /
	///   symbolic_import (Dict, empty when not symbolic: supported,
	///                    project_file_exts (Array<String>),
	///                    tag_name_hints (Array<String>))
	static Dictionary get_tag_field_catalog(const String &p_driver_key);

	/// Synchronous helpers that extract single lists into Godot containers.
	/// Return empty only for symbolic drivers / unknown driver keys.
	static PackedStringArray get_address_type_labels(const String &p_driver_key);
	static PackedStringArray get_address_type_ids(const String &p_driver_key);
	static PackedStringArray get_data_format_msgids(const String &p_driver_key);

private:
	// There are two endpoints to fetch: drivers and address-catalog.
	// We use two HTTPRequest nodes and call both concurrently, then combine
	// the result when both settle (via atomic counters).
	static HTTPRequest *s_http_drivers;
	static HTTPRequest *s_http_s7_addr;
	static int s_pending_requests;

	static bool s_driver_catalog_ready;
	static bool s_address_catalog_ready;

	static PackedStringArray s_s7_address_types;
	static Array s_s7_address_catalog;
	static Array s_driver_catalog;
	static Callable s_fetch_callback;

	// ---- Per-driver tag-field catalog cache (NEW) ----
	// HashMap<String, Dictionary> → cache keyed by driver_key.
	// Using a Dictionary as a map from driver_key -> catalog Dictionary is
	// simpler than instantiating std::unordered_map in the header.
	static Dictionary s_tagfield_cache;           // driver_key -> Dict catalog
	static Dictionary s_tagfield_http;            // driver_key -> HTTPRequest* (as int via Variant())
	static Dictionary s_tagfield_callbacks;       // driver_key -> Callable

	/// Builds the in-memory hard-coded family fallback for p_driver_key.
	/// Used whenever the HTTP API is unavailable or returns an error.
	static Dictionary _tag_field_fallback(const String &p_driver_key);

	/// Static HTTPRequest callback completion: fires once per catalog fetch
	/// for a specific driver.  Looks up p_driver_key via userdata so this
	/// function can remain static.
	// HTTPRequest "request_completed" callback bound with trailing driver_key
	// (Callable::bind appends arguments, so the signature is:
	//   (result, code, headers, body) + bound driver_key).
	static void _on_tagfield_completed(int p_result, int p_response_code,
			const PackedStringArray &p_headers, const PackedByteArray &p_body,
			const String &p_driver_key);

	/// Called by both callbacks when their respective request finishes.
	/// When all pending requests are settled, marks cache ready and invokes
	/// s_fetch_callback once (only once) so the UI can refresh.
	static void _maybe_settle();

	/// Hard-coded fallback used when the runtime is not reachable.
	static PackedStringArray s7_address_types_fallback();

	/// Shared parser for PackedByteArray body → Variant JSON → Dictionary.
	/// Returns an empty Dictionary on any transport/parse error; populates
	/// out_error with a printable reason when the caller cares.
	static Dictionary _parse_response_body(const PackedByteArray &p_body,
			String *r_error = nullptr);

	/// HTTP request completion callbacks (static).
	static void _on_drivers_completed(int p_result, int p_response_code,
			const PackedStringArray &p_headers, const PackedByteArray &p_body);
	static void _on_s7_addr_completed(int p_result, int p_response_code,
			const PackedStringArray &p_headers, const PackedByteArray &p_body);


	static void _on_create_project_completed(int p_result, int p_response_code,
			const PackedStringArray &p_headers, const PackedByteArray &p_body);
	static void _on_import_project_completed(int p_result, int p_response_code,
			const PackedStringArray &p_headers, const PackedByteArray &p_body);

	static Callable s_create_project_success_callback;

	static void _bind_methods();
};
