#include "industrial_runtime.h"
#include "tag_cache.h"
#include "value_convert.h"
#include "ws_client.h"

#include "core/core_globals.h"
#include "core/object/class_db.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "core/string/print_string.h"

IndustrialRuntime *IndustrialRuntime::singleton = nullptr;

IndustrialRuntime *IndustrialRuntime::get_singleton() {
	return singleton;
}

IndustrialRuntime::IndustrialRuntime() {
	cache.instantiate();
	ws.instantiate();

	if (singleton == nullptr) {
		singleton = this;
	}

	// Connect WSClient signals into our bridge lambdas.
	ws->connect("tag_update", callable_mp(this, &IndustrialRuntime::_on_tag_update));
	ws->connect("subscribe_result", callable_mp(this, &IndustrialRuntime::_on_subscribe_result));
	ws->connect("write_result", callable_mp(this, &IndustrialRuntime::_on_write_result));
	ws->connect("connection_state_changed", callable_mp(this, &IndustrialRuntime::_on_connection_state_changed));
	ws->connect("connection_error", callable_mp(this, &IndustrialRuntime::_on_connection_error));
}

IndustrialRuntime::~IndustrialRuntime() {
	if (singleton == this) {
		singleton = nullptr;
	}
	disconnect_runtime();
}

void IndustrialRuntime::_bind_methods() {
	ClassDB::bind_static_method("IndustrialRuntime", D_METHOD("get_singleton"), &IndustrialRuntime::get_singleton);

	ClassDB::bind_method(D_METHOD("connect_runtime", "url"), &IndustrialRuntime::connect_runtime);
	ClassDB::bind_method(D_METHOD("disconnect_runtime"), &IndustrialRuntime::disconnect_runtime);
	ClassDB::bind_method(D_METHOD("process_tick", "delta"), &IndustrialRuntime::process_tick);
	ClassDB::bind_method(D_METHOD("subscribe", "tags", "request_id"), &IndustrialRuntime::subscribe, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("unsubscribe", "tags", "request_id"), &IndustrialRuntime::unsubscribe, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("write_tag", "tag", "value", "request_id"), &IndustrialRuntime::write_tag, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_tag", "tag"), &IndustrialRuntime::get_tag);
	ClassDB::bind_method(D_METHOD("has_tag", "tag"), &IndustrialRuntime::has_tag);
	ClassDB::bind_method(D_METHOD("get_cache"), &IndustrialRuntime::get_cache);
	ClassDB::bind_method(D_METHOD("get_ws"), &IndustrialRuntime::get_ws);
	ClassDB::bind_method(D_METHOD("get_url"), &IndustrialRuntime::get_url);
	ClassDB::bind_method(D_METHOD("get_connection_state"), &IndustrialRuntime::get_connection_state);

	// Internal helpers for TagBinding
	ClassDB::bind_method(D_METHOD("_eval_expression", "expression", "snapshot"), &IndustrialRuntime::_eval_expression);
	ClassDB::bind_method(D_METHOD("_build_snapshot", "tags"), &IndustrialRuntime::_build_snapshot);
	ClassDB::bind_method(D_METHOD("_extract_tags", "expression"), &IndustrialRuntime::_extract_tags);

	ADD_SIGNAL(MethodInfo("tag_changed",
			PropertyInfo(Variant::STRING, "tag"),
			PropertyInfo(Variant::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT),
			PropertyInfo(Variant::STRING, "quality"),
			PropertyInfo(Variant::INT, "version"),
			PropertyInfo(Variant::INT, "ts_ms")));
	ADD_SIGNAL(MethodInfo("subscribe_result",
			PropertyInfo(Variant::STRING, "request_id"),
			PropertyInfo(Variant::BOOL, "ok"),
			PropertyInfo(Variant::STRING, "error")));
	ADD_SIGNAL(MethodInfo("write_result",
			PropertyInfo(Variant::STRING, "request_id"),
			PropertyInfo(Variant::STRING, "tag"),
			PropertyInfo(Variant::BOOL, "ok"),
			PropertyInfo(Variant::STRING, "error"),
			PropertyInfo(Variant::NIL, "verified_value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
	ADD_SIGNAL(MethodInfo("connection_state_changed", PropertyInfo(Variant::INT, "state")));
	ADD_SIGNAL(MethodInfo("connection_error",
			PropertyInfo(Variant::INT, "code"),
			PropertyInfo(Variant::STRING, "message")));
}

void IndustrialRuntime::connect_runtime(const String &p_url) {
	url = p_url.length() > 0 ? p_url : String("ws://127.0.0.1:8787/ws/v1");
	ws->connect_to(url);
}

void IndustrialRuntime::disconnect_runtime() {
	ws->close();
}

void IndustrialRuntime::process_tick(double /*delta*/) {
	ws->poll();
}

bool IndustrialRuntime::subscribe(const Array &tags, const String &request_id) {
	return ws->send_subscribe(tags, request_id);
}

bool IndustrialRuntime::unsubscribe(const Array &tags, const String &request_id) {
	return ws->send_unsubscribe(tags, request_id);
}

bool IndustrialRuntime::write_tag(const String &tag, const Variant &value, const String &request_id) {
	return ws->send_write(tag, value, request_id);
}

Dictionary IndustrialRuntime::get_tag(const String &tag) const {
	return cache->get_entry(tag);
}

bool IndustrialRuntime::has_tag(const String &tag) const {
	return cache->has(tag);
}

int IndustrialRuntime::get_connection_state() const {
	return (int)ws->get_state();
}

void IndustrialRuntime::_on_tag_update(const Array &flat_values) {
	for (int i = 0; i < flat_values.size(); i++) {
		Dictionary flat = flat_values[i];
		const Variant *tag_v = flat.getptr("tag");
		if (tag_v == nullptr || tag_v->get_type() != Variant::STRING) continue;
		String tag_s = String(*tag_v);
		if (tag_s.length() == 0) continue;
		Variant value = flat.get("value", Variant());
		String quality = flat.has("quality") ? String(flat["quality"]) : String("stale");
		int64_t version = flat.has("version") ? int64_t(flat["version"]) : 0;
		int64_t ts_ms = flat.has("ts_ms") ? int64_t(flat["ts_ms"]) : 0;
		cache->set_one(flat);
		// Must be real Variants: casting int64_t* to Variant* is UB and breaks
		// signal arg conversion ("Cannot convert argument 5 from  to int").
		Variant args[5] = { tag_s, value, quality, version, ts_ms };
		const Variant *argv[5] = { &args[0], &args[1], &args[2], &args[3], &args[4] };
		emit_signalp("tag_changed", argv, 5);
	}
}

void IndustrialRuntime::_on_subscribe_result(const String &request_id, bool ok, const String &error) {
	Variant rid = request_id; Variant vok = ok; Variant verr = error;
	const Variant *args[3] = { &rid, &vok, &verr };
	emit_signalp("subscribe_result", args, 3);
}

void IndustrialRuntime::_on_write_result(const String &request_id, const String &tag, bool ok, const String &error, const Variant &verified) {
	Variant rid = request_id; Variant vtag = tag; Variant vok = ok; Variant verr = error;
	const Variant *args[5] = { &rid, &vtag, &vok, &verr, &verified };
	emit_signalp("write_result", args, 5);
}

void IndustrialRuntime::_on_connection_state_changed(int state) {
	Variant v = state;
	const Variant *args[1] = { &v };
	emit_signalp("connection_state_changed", args, 1);
}

void IndustrialRuntime::_on_connection_error(int code, const String &message) {
	Variant vcode = code; Variant vmsg = message;
	const Variant *args[2] = { &vcode, &vmsg };
	emit_signalp("connection_error", args, 2);
}

Dictionary IndustrialRuntime::_build_snapshot(const Array &tags) const {
	Dictionary snap;
	for (int i = 0; i < tags.size(); i++) {
		String t = tags[i];
		snap[t] = cache->get_entry(t);
	}
	return snap;
}

Dictionary IndustrialRuntime::_eval_expression(const String &expression, const Dictionary &snapshot) {
	// ExprEval is not compiled into this module variant; see SCsub / README.
	// Return an "error" Dictionary so TagBinding falls back to its `fallback`
	// value rather than aborting the whole scene.
	Dictionary out;
	out["ok"] = false;
	out["error"] = String("ExprEval not built into this binary; expression=") + expression;
	out["value"] = Variant();
	return out;
}

Array IndustrialRuntime::_extract_tags(const String &expression) {
	// Naive fallback when ExprEval isn't available: scan for $-prefixed names
	// so TagBinding's subscription still works against the cache.
	Array out;
	int i = 0;
	while (i < expression.length()) {
		char32_t c = expression[i];
		if (c == '$' && i + 1 < expression.length()) {
			i++;
			int start = i;
			while (i < expression.length()) {
				char32_t k = expression[i];
				bool cont = (k == '_') || (k == '.') || (k == '/') ||
							(k >= 'a' && k <= 'z') || (k >= 'A' && k <= 'Z') ||
							(k >= '0' && k <= '9');
				if (!cont) break;
				i++;
			}
			out.push_back(expression.substr(start, i - start));
			continue;
		}
		i++;
	}
	return out;
}
