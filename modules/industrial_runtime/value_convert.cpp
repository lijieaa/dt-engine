#include "value_convert.h"

#include "core/core_globals.h"
#include "core/io/json.h"
#include "core/string/print_string.h"

namespace ValueConvert {

Dictionary from_wire_dict(const Dictionary &wire) {
	Dictionary out;
	if (!wire.has("tag") || !wire.has("value")) {
		WARN_PRINT_ONCE("IR: received tag_update entry missing tag/value fields; skipping.");
		return out;
	}
	out["tag"] = wire["tag"];
	out["value"] = narrow_numeric(wire["value"]);
	out["quality"] = wire.has("quality") ? String(wire["quality"]) : String(ir_proto::QUALITY_STALE);
	out["ts_ms"] = wire.has("timestamp_ms") ? int64_t(wire["timestamp_ms"]) : int64_t(0);
	out["version"] = wire.has("version") ? uint64_t(int64_t(wire["version"])) : uint64_t(0);
	return out;
}

Dictionary to_wire_dict(const Dictionary &flat) {
	Dictionary wire;
	wire["tag"] = flat.get("tag", String());
	wire["value"] = flat.get("value", Variant());
	wire["quality"] = flat.get("quality", String(ir_proto::QUALITY_STALE));
	wire["timestamp_ms"] = int64_t(flat.get("ts_ms", int64_t(0)));
	wire["version"] = int64_t(uint64_t(flat.get("version", uint64_t(0))));
	return wire;
}

Dictionary parse_json_text(const String &text) {
	Variant v = JSON::parse_string(text);
	if (v.get_type() == Variant::DICTIONARY) {
		return Dictionary(v);
	}
	return Dictionary();
}

String serialize_json_text(const Dictionary &dict) {
	return JSON::stringify(dict, "", false, true);
}

Dictionary make_message(const String &type, const String &request_id) {
	Dictionary msg;
	msg["version"] = ir_proto::PROTOCOL_VERSION;
	msg["type"] = type;
	if (request_id.length() > 0) {
		msg["request_id"] = request_id;
	}
	return msg;
}

Variant narrow_numeric(const Variant &value) {
	switch (value.get_type()) {
		case Variant::FLOAT: {
			double d = value;
			if (d >= double(INT64_MIN) && d <= double(INT64_MAX)) {
				int64_t i = int64_t(d);
				if (double(i) == d) {
					return i;
				}
			}
			return value;
		}
		case Variant::INT:
		case Variant::STRING:
		case Variant::BOOL:
		case Variant::NIL:
			return value;
		default:
			// Arrays/dicts/objects are passed through untouched.
			return value;
	}
}

} // namespace ValueConvert
