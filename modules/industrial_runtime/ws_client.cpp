#include "ws_client.h"

#include "value_convert.h"

#include "core/core_globals.h"
#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/variant/array.h"
#include "modules/websocket/websocket_peer.h"

WSClient::WSClient() {
	peer.unref();
}

WSClient::~WSClient() {
	close();
}

void WSClient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("connect_to", "url"), &WSClient::connect_to);
	ClassDB::bind_method(D_METHOD("close"), &WSClient::close);
	ClassDB::bind_method(D_METHOD("poll"), &WSClient::poll);
	ClassDB::bind_method(D_METHOD("get_state"), &WSClient::get_state);
	ClassDB::bind_method(D_METHOD("get_url"), &WSClient::get_url);
	ClassDB::bind_method(D_METHOD("send_subscribe", "tags", "request_id"), &WSClient::send_subscribe, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("send_unsubscribe", "tags", "request_id"), &WSClient::send_unsubscribe, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("send_write", "tag", "value", "request_id"), &WSClient::send_write, DEFVAL(String()));

	BIND_ENUM_CONSTANT(STATE_IDLE);
	BIND_ENUM_CONSTANT(STATE_CONNECTING);
	BIND_ENUM_CONSTANT(STATE_OPEN);
	BIND_ENUM_CONSTANT(STATE_CLOSING);
	BIND_ENUM_CONSTANT(STATE_CLOSED);

	ADD_SIGNAL(MethodInfo("connection_state_changed",
			PropertyInfo(Variant::INT, "state")));
	ADD_SIGNAL(MethodInfo("connection_error",
			PropertyInfo(Variant::INT, "close_code"),
			PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("message_received",
			PropertyInfo(Variant::DICTIONARY, "msg")));
	ADD_SIGNAL(MethodInfo("subscribe_result",
			PropertyInfo(Variant::STRING, "request_id"),
			PropertyInfo(Variant::BOOL, "ok"),
			PropertyInfo(Variant::STRING, "error")));
	ADD_SIGNAL(MethodInfo("tag_update",
			PropertyInfo(Variant::ARRAY, "flat_values")));
	ADD_SIGNAL(MethodInfo("write_result",
			PropertyInfo(Variant::STRING, "request_id"),
			PropertyInfo(Variant::STRING, "tag"),
			PropertyInfo(Variant::BOOL, "ok"),
			PropertyInfo(Variant::STRING, "error"),
			PropertyInfo(Variant::NIL, "verified_value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
}

void WSClient::set_state(ConnectionState s, int close_code, const String &close_reason) {
	if (state == s) {
		return;
	}
	state = s;
	Variant s_val = int(s);
	const Variant *args[1] = { &s_val };
	emit_signalp("connection_state_changed", args, 1);
	if (s == STATE_OPEN) {
		flush_desired_subscriptions();
		flush_pending_writes();
	}
	if (s == STATE_CLOSED && (close_code != 1000 && close_code != -1)) {
		Variant code = close_code;
		Variant reason = close_reason;
		const Variant *eargs[2] = { &code, &reason };
		emit_signalp("connection_error", eargs, 2);
	}
}

void WSClient::flush_desired_subscriptions() {
	if (desired_tags.is_empty() || state != STATE_OPEN) {
		return;
	}
	Array tags;
	tags.resize(desired_tags.size());
	int i = 0;
	for (const String &tag : desired_tags) {
		tags[i++] = tag;
	}
	Dictionary msg = ValueConvert::make_message(ir_proto::MSG_TYPE_SUBSCRIBE, String("s") + itos(auto_request_id++));
	msg["tags"] = tags;
	send_text_dict(msg);
}

void WSClient::flush_pending_writes() {
	if (pending_writes.is_empty() || state != STATE_OPEN) {
		return;
	}
	Vector<PendingWrite> batch = pending_writes;
	pending_writes.clear();
	for (const PendingWrite &pw : batch) {
		Dictionary msg = ValueConvert::make_message(ir_proto::MSG_TYPE_WRITE, pw.request_id);
		msg["tag"] = pw.tag;
		msg["value"] = pw.value;
		send_text_dict(msg);
	}
}

void WSClient::connect_to(const String &p_url) {
	url = p_url;
	if (peer.is_null()) {
		peer = Ref<WebSocketPeer>(WebSocketPeer::create());
	}
	if (peer.is_null()) {
		set_state(STATE_CLOSED, -1, "WebSocketPeer factory unavailable (websocket module not built).");
		return;
	}
	if (state == STATE_OPEN || state == STATE_CONNECTING || state == STATE_CLOSING) {
		peer->close(1001, "reconnecting");
		set_state(STATE_CLOSING);
	}
	Error err = peer->connect_to_url(url);
	if (err != OK) {
		set_state(STATE_CLOSED, -1, String("connect_to_url failed, err=") + itos(err));
		return;
	}
	set_state(STATE_CONNECTING);
}

void WSClient::close() {
	if (!peer.is_null() && state != STATE_CLOSED && state != STATE_IDLE) {
		peer->close(1000, "client disconnect");
		set_state(STATE_CLOSING);
	} else {
		set_state(STATE_CLOSED, 1000, "idle disconnected");
	}
}

void WSClient::poll() {
	if (peer.is_null()) {
		return;
	}
	peer->poll();
	WebSocketPeer::State s = peer->get_ready_state();
	ConnectionState mapped = STATE_IDLE;
	switch (s) {
		case WebSocketPeer::STATE_CONNECTING: mapped = STATE_CONNECTING; break;
		case WebSocketPeer::STATE_OPEN:       mapped = STATE_OPEN;       break;
		case WebSocketPeer::STATE_CLOSING:    mapped = STATE_CLOSING;    break;
		case WebSocketPeer::STATE_CLOSED:     mapped = STATE_CLOSED;     break;
	}
	if (mapped != state) {
		set_state(mapped, peer->get_close_code(), peer->get_close_reason());
	}
	drain_incoming();
}

void WSClient::drain_incoming() {
	if (peer.is_null()) {
		return;
	}
	while (peer->get_available_packet_count() > 0) {
		const uint8_t *raw = nullptr;
		int len = 0;
		Error pkt_err = peer->get_packet(&raw, len);
		if (pkt_err != OK || raw == nullptr || len <= 0) {
			continue;
		}
		String text;
		if (peer->was_string_packet()) {
			text = String::utf8(reinterpret_cast<const char *>(raw), len);
		} else {
			// Protocol uses JSON text; ignore stray binary frames.
			continue;
		}
		Dictionary msg = ValueConvert::parse_json_text(text);
		if (msg.is_empty()) {
			continue;
		}
		Variant msg_v = msg;
		const Variant *args[1] = { &msg_v };
		emit_signalp("message_received", args, 1);
		handle_message(msg);
	}
}

void WSClient::handle_message(const Dictionary &msg) {
	String type = msg.get("type", String());
	String request_id = msg.get("request_id", String());

	if (type == ir_proto::MSG_TYPE_TAG_UPDATE) {
		Array values = msg.get("values", Array());
		Array flats;
		flats.resize(values.size());
		for (int i = 0; i < values.size(); i++) {
			flats[i] = ValueConvert::from_wire_dict(values[i]);
		}
		Variant flats_v = flats;
		const Variant *args[1] = { &flats_v };
		emit_signalp("tag_update", args, 1);
		return;
	}

	if (type == ir_proto::MSG_TYPE_SUBSCRIBE_RESULT) {
		bool ok = msg.get("ok", false);
		String err = msg.get("error", String());
		Variant rid = request_id;
		Variant okv = ok;
		Variant errv = err;
		const Variant *args[3] = { &rid, &okv, &errv };
		emit_signalp("subscribe_result", args, 3);
		return;
	}

	if (type == ir_proto::MSG_TYPE_WRITE_RESULT) {
		String tag = msg.get("tag", String());
		bool ok = msg.get("ok", false);
		String err = msg.get("error", String());
		Variant verified = Variant();
		// write_result currently does not include the verified value; we keep
		// the signal parameter so future versions can simply include it here
		// without breaking the GDScript call sites.
		Variant rid = request_id;
		Variant tagv = tag;
		Variant okv = ok;
		Variant errv = err;
		const Variant *args[5] = { &rid, &tagv, &okv, &errv, &verified };
		emit_signalp("write_result", args, 5);
		return;
	}
}

bool WSClient::send_text_dict(const Dictionary &msg) {
	if (peer.is_null() || state != STATE_OPEN) {
		return false;
	}
	String text = ValueConvert::serialize_json_text(msg);
	Error err = peer->send_text(text);
	return err == OK;
}

bool WSClient::send_subscribe(const Array &tags, const String &request_id) {
	for (int i = 0; i < tags.size(); i++) {
		const String tag = String(tags[i]).strip_edges();
		if (!tag.is_empty()) {
			desired_tags.insert(tag);
		}
	}
	if (state != STATE_OPEN) {
		// Remembered in desired_tags; flushed when the socket opens.
		return true;
	}
	String rid = request_id.length() > 0 ? request_id : String("s") + itos(auto_request_id++);
	Dictionary msg = ValueConvert::make_message(ir_proto::MSG_TYPE_SUBSCRIBE, rid);
	msg["tags"] = tags;
	return send_text_dict(msg);
}

bool WSClient::send_unsubscribe(const Array &tags, const String &request_id) {
	for (int i = 0; i < tags.size(); i++) {
		desired_tags.erase(String(tags[i]).strip_edges());
	}
	if (state != STATE_OPEN) {
		return true;
	}
	String rid = request_id.length() > 0 ? request_id : String("u") + itos(auto_request_id++);
	Dictionary msg = ValueConvert::make_message(ir_proto::MSG_TYPE_UNSUBSCRIBE, rid);
	msg["tags"] = tags;
	return send_text_dict(msg);
}

String WSClient::json_value(const Variant &v) {
	if (v.get_type() == Variant::NIL) {
		return "null";
	}
	return JSON::stringify(v, "", false, true);
}

bool WSClient::send_write(const String &tag, const Variant &value, const String &request_id) {
	String rid = request_id.length() > 0 ? request_id : String("w") + itos(auto_request_id++);
	if (state != STATE_OPEN) {
		PendingWrite pw;
		pw.tag = tag;
		pw.value = value;
		pw.request_id = rid;
		pending_writes.push_back(pw);
		return true;
	}
	// Reconstruct as JSON dictionary. Because `value` could be any Variant we
	// can't just stick it into `msg["value"]` — that would emit its native
	// JSON encoding (which is what we want). Actually yes, that works:
	// `msg["value"] = value` serializes correctly through JSON::stringify.
	Dictionary msg = ValueConvert::make_message(ir_proto::MSG_TYPE_WRITE, rid);
	msg["tag"] = tag;
	msg["value"] = value;
	return send_text_dict(msg);
}
