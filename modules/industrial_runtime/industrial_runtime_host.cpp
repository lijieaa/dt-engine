#include "industrial_runtime_host.h"

#include "industrial_runtime.h"
#include "ws_client.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "core/string/print_string.h"
#include "scene/main/http_request.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

IndustrialRuntimeHost::IndustrialRuntimeHost() {
	runtime.instantiate();
	runtime->connect("tag_changed", callable_mp(this, &IndustrialRuntimeHost::_on_runtime_tag_changed));
	runtime->connect("connection_state_changed", callable_mp(this, &IndustrialRuntimeHost::_on_runtime_state_changed));
	runtime->connect("connection_error", callable_mp(this, &IndustrialRuntimeHost::_on_runtime_connection_error));
	runtime->connect("subscribe_result", callable_mp(this, &IndustrialRuntimeHost::_on_runtime_subscribe_result));
	runtime->connect("write_result", callable_mp(this, &IndustrialRuntimeHost::_on_runtime_write_result));
}

IndustrialRuntimeHost::~IndustrialRuntimeHost() {
	_stop();
}

void IndustrialRuntimeHost::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_session_id", "id"), &IndustrialRuntimeHost::set_session_id);
	ClassDB::bind_method(D_METHOD("get_session_id"), &IndustrialRuntimeHost::get_session_id);
	ClassDB::bind_method(D_METHOD("subscribe", "tags", "request_id"), &IndustrialRuntimeHost::subscribe, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("unsubscribe", "tags", "request_id"), &IndustrialRuntimeHost::unsubscribe, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("write_tag", "tag", "value", "request_id"), &IndustrialRuntimeHost::write_tag, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_tag", "tag"), &IndustrialRuntimeHost::get_tag);
	ClassDB::bind_method(D_METHOD("has_tag", "tag"), &IndustrialRuntimeHost::has_tag);
	ClassDB::bind_method(D_METHOD("get_connection_state"), &IndustrialRuntimeHost::get_connection_state);
	ClassDB::bind_static_method("IndustrialRuntimeHost", D_METHOD("ensure_at_root", "from"), &IndustrialRuntimeHost::ensure_at_root);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "session_id"), "set_session_id", "get_session_id");

	ADD_SIGNAL(MethodInfo("tag_changed",
			PropertyInfo(Variant::STRING, "tag"),
			PropertyInfo(Variant::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT),
			PropertyInfo(Variant::STRING, "quality"),
			PropertyInfo(Variant::INT, "version"),
			PropertyInfo(Variant::INT, "ts_ms")));
	ADD_SIGNAL(MethodInfo("connection_state_changed", PropertyInfo(Variant::INT, "state")));
	ADD_SIGNAL(MethodInfo("connection_error",
			PropertyInfo(Variant::INT, "code"),
			PropertyInfo(Variant::STRING, "message")));
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
}

void IndustrialRuntimeHost::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			Node *parent = get_parent();
			if (parent != nullptr && parent->has_meta(SNAME("_irt_host_pending"))) {
				parent->remove_meta(SNAME("_irt_host_pending"));
			}
			if (http == nullptr) {
				http = memnew(HTTPRequest);
				http->set_timeout(30.0);
				add_child(http);
				http->connect("request_completed", callable_mp(this, &IndustrialRuntimeHost::_on_http_done));
			}
			_start();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			_stop();
		} break;
		case NOTIFICATION_PROCESS: {
			if (runtime.is_valid()) {
				runtime->process_tick(get_process_delta_time());
				const int st = runtime->get_connection_state();
				if (st == WSClient::STATE_IDLE || st == WSClient::STATE_CLOSED) {
					const int64_t now = OS::get_singleton()->get_ticks_msec();
					if (now >= next_reconnect_ms) {
						_connect_ws();
					}
				}
			}
		} break;
		default:
			break;
	}
}

Node *IndustrialRuntimeHost::ensure_at_root(Node *p_from) {
	if (p_from == nullptr || !p_from->is_inside_tree()) {
		return nullptr;
	}
	// Editor canvas is driven by IndustrialEditorPlugin tag push; do not
	// inject a play-session host into the editor SceneTree.
	if (Engine::get_singleton()->is_editor_hint()) {
		return p_from->get_tree()->get_root()->get_node_or_null(NodePath("/root/Runtime"));
	}
	SceneTree *tree = p_from->get_tree();
	if (tree == nullptr) {
		return nullptr;
	}
	Window *root = tree->get_root();
	if (root == nullptr) {
		return nullptr;
	}
	Node *existing = root->get_node_or_null(NodePath("Runtime"));
	if (existing != nullptr) {
		return existing;
	}
	// Scene may still be building children (Tag* READY); add_child must be deferred.
	const StringName pending_key = SNAME("_irt_host_pending");
	if (root->has_meta(pending_key)) {
		const Variant meta = root->get_meta(pending_key);
		Object *obj = meta.get_type() == Variant::OBJECT ? meta.operator Object *() : nullptr;
		IndustrialRuntimeHost *pending = Object::cast_to<IndustrialRuntimeHost>(obj);
		if (pending != nullptr) {
			return pending;
		}
	}
	IndustrialRuntimeHost *host = memnew(IndustrialRuntimeHost);
	host->set_name("Runtime");
	root->set_meta(pending_key, host);
	root->call_deferred(SNAME("add_child"), host);
	print_line("industrial_runtime: spawned Runtime host (C++, deferred add)");
	return host;
}

void IndustrialRuntimeHost::_start() {
	if (started) {
		return;
	}
	started = true;
	set_process(true);
	_load_project_json();
	_import_project();
}

void IndustrialRuntimeHost::_stop() {
	if (!started) {
		return;
	}
	started = false;
	set_process(false);
	_collect_release();
	if (runtime.is_valid()) {
		runtime->disconnect_runtime();
	}
}

void IndustrialRuntimeHost::_load_project_json() {
	project_json_path = "res://industrial/project.json";
	project_json_body.clear();
	ProjectSettings *ps = ProjectSettings::get_singleton();
	if (ps != nullptr && ps->has_setting("industrial/project/data_path")) {
		const String custom = ps->get_setting("industrial/project/data_path");
		if (!custom.is_empty()) {
			project_json_path = custom;
		}
	}
	Ref<FileAccess> f = FileAccess::open(project_json_path, FileAccess::READ);
	if (f.is_null()) {
		WARN_PRINT(vformat("industrial_runtime: host missing %s", project_json_path));
		return;
	}
	project_json_body = f->get_as_text();
	const Variant parsed = JSON::parse_string(project_json_body);
	if (parsed.get_type() != Variant::DICTIONARY) {
		WARN_PRINT(vformat("industrial_runtime: host %s is not valid JSON", project_json_path));
		project_json_body.clear();
		return;
	}
	const String url = String(Dictionary(parsed).get("runtime_url", "")).strip_edges();
	if (!url.is_empty()) {
		http_base_url = url.trim_suffix("/");
		ws_url = _http_to_ws(http_base_url);
		print_line(vformat("industrial_runtime: host url from project.json -> %s", http_base_url));
	}
}

String IndustrialRuntimeHost::_http_to_ws(const String &p_http) {
	String u = p_http.strip_edges().trim_suffix("/");
	if (u.begins_with("https://")) {
		return String("wss://") + u.substr(8) + "/ws/v1";
	}
	if (u.begins_with("http://")) {
		return String("ws://") + u.substr(7) + "/ws/v1";
	}
	return String("ws://") + u + "/ws/v1";
}

void IndustrialRuntimeHost::_import_project() {
	if (http == nullptr) {
		_connect_ws();
		_collect_acquire();
		return;
	}
	if (project_json_body.is_empty()) {
		WARN_PRINT("industrial_runtime: host skip import (empty project.json), connect+collect only");
		_connect_ws();
		_collect_acquire();
		return;
	}
	http_phase = HTTP_IMPORT;
	const String url = http_base_url + "/api/v1/project/import";
	print_line(vformat("industrial_runtime: host POST %s (import, %d bytes)", url, project_json_body.length()));
	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	const Error err = http->request(url, headers, HTTPClient::METHOD_POST, project_json_body);
	if (err != OK) {
		WARN_PRINT(vformat("industrial_runtime: host import request failed: %d", (int)err));
		http_phase = HTTP_NONE;
		_connect_ws();
		_collect_acquire();
	}
}

void IndustrialRuntimeHost::_connect_ws() {
	if (runtime.is_null()) {
		return;
	}
	next_reconnect_ms = OS::get_singleton()->get_ticks_msec() + reconnect_backoff_ms;
	print_line(vformat("industrial_runtime: host connect %s", ws_url));
	runtime->connect_runtime(ws_url);
}

void IndustrialRuntimeHost::_collect_acquire() {
	if (collect_acquired || http == nullptr || session_id.is_empty()) {
		return;
	}
	http_phase = HTTP_COLLECT_ACQUIRE;
	const String url = http_base_url + "/api/v1/runtime/collect";
	Dictionary body;
	body["action"] = "acquire";
	body["session_id"] = session_id;
	const String payload = JSON::stringify(body);
	print_line(vformat("industrial_runtime: host POST %s acquire=%s", url, session_id));
	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	const Error err = http->request(url, headers, HTTPClient::METHOD_POST, payload);
	if (err != OK) {
		WARN_PRINT(vformat("industrial_runtime: collect acquire request failed: %d", (int)err));
		http_phase = HTTP_NONE;
	}
}

void IndustrialRuntimeHost::_collect_release() {
	if (!collect_acquired || http == nullptr || session_id.is_empty()) {
		return;
	}
	collect_acquired = false;
	http_phase = HTTP_COLLECT_RELEASE;
	const String url = http_base_url + "/api/v1/runtime/collect";
	Dictionary body;
	body["action"] = "release";
	body["session_id"] = session_id;
	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	http->request(url, headers, HTTPClient::METHOD_POST, JSON::stringify(body));
}

void IndustrialRuntimeHost::_on_http_done(int p_result, int p_code, const PackedStringArray &, const PackedByteArray &p_body) {
	const HttpPhase phase = http_phase;
	http_phase = HTTP_NONE;
	const String body = String::utf8((const char *)p_body.ptr(), p_body.size());

	if (phase == HTTP_IMPORT) {
		if (p_result == OK && p_code == 200) {
			print_line(vformat("industrial_runtime: host import OK %s", body));
		} else {
			WARN_PRINT(vformat("industrial_runtime: host import failed result=%d http=%d body=%s", p_result, p_code, body));
		}
		// Collect + WS even if import failed (server may already have the project).
		_connect_ws();
		_collect_acquire();
		return;
	}

	if (phase == HTTP_COLLECT_ACQUIRE) {
		if (p_result == OK && p_code == 200) {
			collect_acquired = true;
			print_line(vformat("industrial_runtime: host collect OK %s", body));
		} else {
			WARN_PRINT(vformat("industrial_runtime: host collect failed result=%d http=%d body=%s", p_result, p_code, body));
		}
		return;
	}

	if (phase == HTTP_COLLECT_RELEASE) {
		if (!(p_result == OK && p_code == 200)) {
			WARN_PRINT(vformat("industrial_runtime: host collect release failed result=%d http=%d", p_result, p_code));
		}
	}
}

bool IndustrialRuntimeHost::subscribe(const Array &p_tags, const String &p_request_id) {
	return runtime.is_valid() ? runtime->subscribe(p_tags, p_request_id) : false;
}

bool IndustrialRuntimeHost::unsubscribe(const Array &p_tags, const String &p_request_id) {
	return runtime.is_valid() ? runtime->unsubscribe(p_tags, p_request_id) : false;
}

bool IndustrialRuntimeHost::write_tag(const String &p_tag, const Variant &p_value, const String &p_request_id) {
	return runtime.is_valid() ? runtime->write_tag(p_tag, p_value, p_request_id) : false;
}

Dictionary IndustrialRuntimeHost::get_tag(const String &p_tag) const {
	return runtime.is_valid() ? runtime->get_tag(p_tag) : Dictionary();
}

bool IndustrialRuntimeHost::has_tag(const String &p_tag) const {
	return runtime.is_valid() ? runtime->has_tag(p_tag) : false;
}

int IndustrialRuntimeHost::get_connection_state() const {
	return runtime.is_valid() ? runtime->get_connection_state() : 0;
}

void IndustrialRuntimeHost::_on_runtime_tag_changed(const String &p_tag, const Variant &p_value, const String &p_quality, int64_t p_version, int64_t p_ts_ms) {
	emit_signal(SNAME("tag_changed"), p_tag, p_value, p_quality, p_version, p_ts_ms);
}

void IndustrialRuntimeHost::_on_runtime_state_changed(int p_state) {
	if (p_state == WSClient::STATE_OPEN) {
		print_line("industrial_runtime: host WS open");
	}
	emit_signal(SNAME("connection_state_changed"), p_state);
}

void IndustrialRuntimeHost::_on_runtime_connection_error(int p_code, const String &p_message) {
	emit_signal(SNAME("connection_error"), p_code, p_message);
}

void IndustrialRuntimeHost::_on_runtime_subscribe_result(const String &p_rid, bool p_ok, const String &p_error) {
	emit_signal(SNAME("subscribe_result"), p_rid, p_ok, p_error);
}

void IndustrialRuntimeHost::_on_runtime_write_result(const String &p_rid, const String &p_tag, bool p_ok, const String &p_error, const Variant &p_verified) {
	emit_signal(SNAME("write_result"), p_rid, p_tag, p_ok, p_error, p_verified);
}
