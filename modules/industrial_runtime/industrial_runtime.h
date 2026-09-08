#pragma once

#include "tag_cache.h"
#include "ws_client.h"

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

// IndustrialRuntime is the GDScript-visible facade for the industrial_runtime
// module. A scene host (IndustrialRuntimeHost) or editor plugin owns one
// instance. It owns:
//
//   * TagCache     — local tag value mirror, refreshed by tag_update frames.
//   * WSClient     — JSON protocol pipe to the Go driver-engine runtime.
//
// All of the above are also exposed individually as properties so advanced
// scripts can inspect raw state.
class IndustrialRuntime : public RefCounted {
	GDCLASS(IndustrialRuntime, RefCounted);

public:
	IndustrialRuntime();
	~IndustrialRuntime() override;

	// Static accessors ---------------------------------------------------

	// Returns the module-level singleton. This mirrors the autoload pattern
	// from GDScript but works from other C++ classes too.
	static IndustrialRuntime *get_singleton();

	// API ---------------------------------------------------------------

	// Connect the WS client. `p_url` defaults to
	// "ws://127.0.0.1:8787/ws/v1" but any ws[s]:// URL works.
	void connect_runtime(const String &p_url);

	// Disconnect. Safe to call even when not connected.
	void disconnect_runtime();

	// Drive connection IO: must be called once per frame. The Runtime.gd
	// autoload hooks _process to call this.
	void process_tick(double delta);

	// Subscribe to an array of tag names. Already-subscribed tags are
	// deduped on the server side, so repeated calls are cheap.
	bool subscribe(const Array &tags, const String &request_id = String());

	// Unsubscribe an array of tag names.
	bool unsubscribe(const Array &tags, const String &request_id = String());

	// Submit a write command. The tag must be `writable=true` server-side;
	// the result is delivered via write_result signal.
	bool write_tag(const String &tag, const Variant &value, const String &request_id = String());

	// Convenience getter that walks cache → returns flat Dict for `tag`.
	Dictionary get_tag(const String &tag) const;
	bool has_tag(const String &tag) const;

	// Accessors for debugging / advanced scripts.
	Ref<TagCache> get_cache() const { return cache; }
	Ref<WSClient> get_ws() const { return ws; }
	String get_url() const { return url; }
	int get_connection_state() const;

	// Public helpers (used by TagBinding when that module is enabled; kept
	// here so the facade's contract is stable across module variants).
	Dictionary _build_snapshot(const Array &tags) const;
	Dictionary _eval_expression(const String &expression, const Dictionary &snapshot);
	Array _extract_tags(const String &expression);

protected:
	static void _bind_methods();

private:
	static IndustrialRuntime *singleton;

	String url;
	Ref<TagCache> cache;
	Ref<WSClient> ws;

	// Signal bridges from WSClient up to this facade, so GDScripts only need
	// to subscribe to IndustrialRuntime signals.
	void _on_tag_update(const Array &flat_values);
	void _on_subscribe_result(const String &request_id, bool ok, const String &error);
	void _on_write_result(const String &request_id, const String &tag, bool ok, const String &error, const Variant &verified);
	void _on_connection_state_changed(int state);
	void _on_connection_error(int code, const String &message);
};
