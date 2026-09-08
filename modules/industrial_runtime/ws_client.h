#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/hash_set.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/type_info.h"

class WebSocketPeer;

// WSClient owns the Godot WebSocketPeer connection and implements the driver-
// engine v1 JSON protocol. Objects of this class are owned by
// IndustrialRuntime; GDScripts talk to the runtime facade instead.
class WSClient : public RefCounted {
	GDCLASS(WSClient, RefCounted);

public:
	enum ConnectionState {
		STATE_IDLE = 0,
		STATE_CONNECTING = 1,
		STATE_OPEN = 2,
		STATE_CLOSING = 3,
		STATE_CLOSED = 4,
	};

	WSClient();
	~WSClient() override;

	// Lifecycle ----------------------------------------------------------

	// Connect to the given WebSocket URL (e.g. "ws://127.0.0.1:8787/ws").
	// STATE_CONNECTING is entered immediately; a connection_error or
	// connection_state_changed(state) signal is emitted later.
	void connect_to(const String &url);

	// Gracefully close the socket. Safe to call when not open.
	// Drop the WebSocket connection. Renamed from `disconnect` to avoid
	// clashing with Object::disconnect (signal disconnection) on ClassDB.
	void close();

	// Must be called periodically to drive the underlying peer. In practice
	// IndustrialRuntime calls it from its Node._process.
	void poll();

	ConnectionState get_state() const { return state; }
	String get_url() const { return url; }

	// Protocol commands ---------------------------------------------------

	// Send `{"type":"subscribe","tags":[...]}`. Returns true when the message
	// was queued into the websocket peer (not when the server ACKed it).
	bool send_subscribe(const Array &tags, const String &request_id = String());

	// Send `{"type":"unsubscribe","tags":[...]}`.
	bool send_unsubscribe(const Array &tags, const String &request_id = String());

	// Send `{"type":"write","tag":...,"value":<json>}`. `value` is serialized
	// to JSON via the standard JSON encoder so number/bool/string/array/dict
	// all flow through unchanged.
	bool send_write(const String &tag, const Variant &value, const String &request_id = String());

protected:
	static void _bind_methods();

private:
	String url;
	mutable Ref<WebSocketPeer> peer;
	ConnectionState state = STATE_IDLE;
	uint32_t auto_request_id = 1; // Only used when caller omits request_id.
	// Tags requested while the socket was down; flushed on every OPEN.
	HashSet<String> desired_tags;
	struct PendingWrite {
		String tag;
		Variant value;
		String request_id;
	};
	Vector<PendingWrite> pending_writes;

	// Transition helpers: set state, emit connection_state_changed and — if
	// the new state is STATE_CLOSED due to an error — emit connection_error.
	void set_state(ConnectionState s, int close_code = 0, const String &close_reason = String());

	// Re-send desired_tags once the peer is OPEN.
	void flush_desired_subscriptions();
	void flush_pending_writes();

	// Send a text frame containing JSON-encoded `msg`.
	bool send_text_dict(const Dictionary &msg);

	// Read all available text packets from the peer and dispatch each one to
	// handle_message().
	void drain_incoming();

	// Route one parsed JSON frame to signals based on msg["type"].
	void handle_message(const Dictionary &msg);

	// Serialize variant to a json-compatible string. Uses JSON::stringify.
	// For Variant() (nil) we output "null" explicitly to satisfy Go side.
	static String json_value(const Variant &v);
};

VARIANT_ENUM_CAST(WSClient::ConnectionState);
