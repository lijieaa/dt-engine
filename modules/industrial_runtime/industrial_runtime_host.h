#pragma once

#include "industrial_runtime.h"

#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include "scene/main/node.h"

class HTTPRequest;

/// Scene-tree host for IndustrialRuntime (C++ replacement for Runtime.gd).
/// On start: load res://industrial/project.json -> POST /project/import ->
/// collect(player) -> WS connect. Re-emits tag_changed for Tag* widgets.
class IndustrialRuntimeHost : public Node {
	GDCLASS(IndustrialRuntimeHost, Node);

public:
	IndustrialRuntimeHost();
	~IndustrialRuntimeHost() override;

	void set_session_id(const String &p_id) { session_id = p_id; }
	String get_session_id() const { return session_id; }

	bool subscribe(const Array &p_tags, const String &p_request_id = String());
	bool unsubscribe(const Array &p_tags, const String &p_request_id = String());
	bool write_tag(const String &p_tag, const Variant &p_value, const String &p_request_id = String());
	Dictionary get_tag(const String &p_tag) const;
	bool has_tag(const String &p_tag) const;
	int get_connection_state() const;

	/// Ensure a host named "Runtime" exists under the scene-tree root.
	/// No-op / returns existing node when already present. Does not create
	/// hosts while Engine::is_editor_hint() (editor canvas uses plugin push).
	static Node *ensure_at_root(Node *p_from);

protected:
	static void _bind_methods();
	void _notification(int p_what);

private:
	enum HttpPhase {
		HTTP_NONE = 0,
		HTTP_IMPORT,
		HTTP_COLLECT_ACQUIRE,
		HTTP_COLLECT_RELEASE,
	};

	Ref<IndustrialRuntime> runtime;
	HTTPRequest *http = nullptr;
	String http_base_url = "http://127.0.0.1:8080";
	String ws_url = "ws://127.0.0.1:8080/ws/v1";
	String session_id = "player";
	String project_json_path = "res://industrial/project.json";
	String project_json_body;
	HttpPhase http_phase = HTTP_NONE;
	bool collect_acquired = false;
	bool started = false;
	int reconnect_backoff_ms = 2000;
	int64_t next_reconnect_ms = 0;

	void _start();
	void _stop();
	void _load_project_json();
	void _import_project();
	void _connect_ws();
	void _collect_acquire();
	void _collect_release();
	void _on_http_done(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _on_runtime_tag_changed(const String &p_tag, const Variant &p_value, const String &p_quality, int64_t p_version, int64_t p_ts_ms);
	void _on_runtime_state_changed(int p_state);
	void _on_runtime_connection_error(int p_code, const String &p_message);
	void _on_runtime_subscribe_result(const String &p_rid, bool p_ok, const String &p_error);
	void _on_runtime_write_result(const String &p_rid, const String &p_tag, bool p_ok, const String &p_error, const Variant &p_verified);

	static String _http_to_ws(const String &p_http);
};
