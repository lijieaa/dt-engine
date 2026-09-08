#pragma once

#include "core/string/ustring.h"
#include "core/variant/variant.h"
#include "core/variant/dictionary.h"

namespace ir_proto {

// Mirrors driver-engine/internal/ws/protocol.go ProtocolVersion.
// Any change to either side must bump both.
constexpr int PROTOCOL_VERSION = 1;

// Message type strings (byte-for-byte with the Go hub).
constexpr const char *MSG_TYPE_SUBSCRIBE = "subscribe";
constexpr const char *MSG_TYPE_SUBSCRIBE_RESULT = "subscribe_result";
constexpr const char *MSG_TYPE_TAG_UPDATE = "tag_update";
constexpr const char *MSG_TYPE_WRITE = "write";
constexpr const char *MSG_TYPE_WRITE_RESULT = "write_result";
constexpr const char *MSG_TYPE_UNSUBSCRIBE = "unsubscribe";

// Quality code strings. Exact matches for driver::Quality.
constexpr const char *QUALITY_GOOD = "good";
constexpr const char *QUALITY_BAD = "bad";
constexpr const char *QUALITY_UNCERTAIN = "uncertain";
constexpr const char *QUALITY_STALE = "stale";

} // namespace ir_proto

// Utility functions shared across the C++ module to keep call sites free of
// JSON/Variant marshaling boilerplate.
namespace ValueConvert {

// Convert a JSON-decoded tag-value Dictionary (wire form: tag/value/quality/
// timestamp_ms/version) into the canonical {key,value,quality,ts_ms,version}
// flat Dictionary used by Godot-side code. Returns an empty Dictionary on
// missing fields and emits a WARN_PRINT so the caller does not need to crash.
Dictionary from_wire_dict(const Dictionary &wire);

// Inverse: convert from the flat Dictionary back to the wire Dict so we can
// round-trip debug prints/tests.
Dictionary to_wire_dict(const Dictionary &flat);

// Parse a JSON text frame into a Dictionary. Returns empty Dict on failure
// (Godot's JSON API already reports errors through its parsed return value).
Dictionary parse_json_text(const String &text);

// Serialize a Dictionary to pretty-compact JSON text (newlines stripped).
String serialize_json_text(const Dictionary &dict);

// Produce {"version": PROTOCOL_VERSION, "type": ...} header plus any extra
// fields. Used by WSClient::send_* helpers.
Dictionary make_message(const String &type, const String &request_id = String());

// Numeric range safety. Tag values on the Go side are int/float; JavaScript
// numbers and Variant doubles share the 53-bit integer mantissa constraint.
// This helper narrows to int64 when the double has no fractional bits; callers
// that really need a plain int can do a range check afterward.
Variant narrow_numeric(const Variant &value);

} // namespace ValueConvert
