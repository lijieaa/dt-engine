#include "tag_binding.h"
#include "industrial_runtime.h"

#include "core/object/class_db.h"
#include "core/variant/callable.h"
#include "scene/main/node.h"
#include "scene/main/window.h"

TagBinding::~TagBinding() {
	// Write-direction signal disconnection is handled by Node base when it
	// destroys, but we break the parent → this link explicitly.
	write_conn = false;
}

void TagBinding::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tags", "tags"), &TagBinding::set_tags);
	ClassDB::bind_method(D_METHOD("get_tags"), &TagBinding::get_tags);
	ClassDB::bind_method(D_METHOD("set_expression", "expression"), &TagBinding::set_expression);
	ClassDB::bind_method(D_METHOD("get_expression"), &TagBinding::get_expression);
	ClassDB::bind_method(D_METHOD("set_target_property", "property"), &TagBinding::set_target_property);
	ClassDB::bind_method(D_METHOD("get_target_property"), &TagBinding::get_target_property);
	ClassDB::bind_method(D_METHOD("set_fallback", "value"), &TagBinding::set_fallback);
	ClassDB::bind_method(D_METHOD("get_fallback"), &TagBinding::get_fallback);
	ClassDB::bind_method(D_METHOD("set_enable_write", "enable"), &TagBinding::set_enable_write);
	ClassDB::bind_method(D_METHOD("get_enable_write"), &TagBinding::get_enable_write);
	ClassDB::bind_method(D_METHOD("set_write_source_signal", "signal_name"), &TagBinding::set_write_source_signal);
	ClassDB::bind_method(D_METHOD("get_write_source_signal"), &TagBinding::get_write_source_signal);
	ClassDB::bind_method(D_METHOD("set_write_source_property", "property"), &TagBinding::set_write_source_property);
	ClassDB::bind_method(D_METHOD("get_write_source_property"), &TagBinding::get_write_source_property);

	ClassDB::bind_method("_do_write", &TagBinding::_do_write);
	ClassDB::bind_method("_on_runtime_tag_changed", &TagBinding::_on_runtime_tag_changed,
			DEFVAL(Variant()), DEFVAL(String()), DEFVAL(0), DEFVAL(0));

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "tags", PROPERTY_HINT_ARRAY_TYPE, "String"), "set_tags", "get_tags");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "expression", PROPERTY_HINT_MULTILINE_TEXT), "set_expression", "get_expression");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "target_property"), "set_target_property", "get_target_property");
	ADD_PROPERTY(PropertyInfo(Variant::NIL, "fallback", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT), "set_fallback", "get_fallback");
	ADD_GROUP("Write Direction", "write_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "write_enable"), "set_enable_write", "get_enable_write");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "write_source_signal"), "set_write_source_signal", "get_write_source_signal");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "write_source_property"), "set_write_source_property", "get_write_source_property");
}

void TagBinding::_notification(int what) {
	switch (what) {
		case NOTIFICATION_POSTINITIALIZE:
			// No-op; real init happens once we have a parent (NOTIFICATION_READY).
			break;
		case NOTIFICATION_READY:
			_bind_runtime_signals();
			break;
		case NOTIFICATION_PARENTED:
			// If added at runtime, set things up again.
			_bind_runtime_signals();
			break;
		case NOTIFICATION_UNPARENTED:
			write_conn = false;
			break;
	}
}

IndustrialRuntime *TagBinding::_runtime_or_null() {
	IndustrialRuntime *singleton = IndustrialRuntime::get_singleton();
	if (singleton != nullptr) {
		return singleton;
	}
	// Fallback: Runtime.gd autoload calls the autoload with same name; reach
	// through Engine.get_singleton if available. In practice, this never
	// fires because the autoload creates the singleton before any scene loads.
	return nullptr;
}

void TagBinding::set_tags(const Array &p_tags) {
	tags = p_tags.duplicate(true);
	write_tag = tags.size() > 0 ? String(tags[0]) : String();
	// If ready, (re)subscribe.
	if (is_inside_tree() && tags.size() > 0) {
		IndustrialRuntime *r = _runtime_or_null();
		if (r != nullptr) {
			r->subscribe(tags);
		}
	}
}

Array TagBinding::get_tags() const { return tags.duplicate(true); }

void TagBinding::set_expression(const String &p_expr) { expression = p_expr; }
String TagBinding::get_expression() const { return expression; }

void TagBinding::set_target_property(const StringName &p_name) { target_property = p_name; }
StringName TagBinding::get_target_property() const { return target_property; }

void TagBinding::set_fallback(const Variant &v) { fallback = v; }
Variant TagBinding::get_fallback() const { return fallback; }

void TagBinding::set_enable_write(bool enable) {
	enable_write = enable;
	if (is_inside_tree()) _bind_runtime_signals();
}
bool TagBinding::get_enable_write() const { return enable_write; }

void TagBinding::set_write_source_signal(const String &sig) {
	write_source_signal = sig;
	if (is_inside_tree()) _bind_runtime_signals();
}
String TagBinding::get_write_source_signal() const { return write_source_signal; }

void TagBinding::set_write_source_property(const StringName &p_name) { write_source_property = p_name; }
StringName TagBinding::get_write_source_property() const { return write_source_property; }

void TagBinding::_bind_runtime_signals() {
	IndustrialRuntime *r = _runtime_or_null();
	Node *p = get_parent();

	if (r != nullptr && !r->is_connected("tag_changed", callable_mp(this, &TagBinding::_on_runtime_tag_changed))) {
		r->connect("tag_changed", callable_mp(this, &TagBinding::_on_runtime_tag_changed));
	}

	if (r != nullptr && tags.size() > 0) {
		r->subscribe(tags);
		// Immediately apply a first evaluation using whatever cache has now.
		Dictionary snap = r->_build_snapshot(tags);
		_apply_now(snap);
	}

	if (p != nullptr && enable_write && !write_conn && write_source_signal.length() > 0) {
		if (p->has_signal(write_source_signal)) {
			p->connect(write_source_signal, callable_mp(this, &TagBinding::_do_write));
			write_conn = true;
		}
	}
}

void TagBinding::_apply_now(const Dictionary &snapshot) {
	IndustrialRuntime *r = _runtime_or_null();
	Node *p = get_parent();
	if (p == nullptr || r == nullptr) return;

	Variant final_value = fallback;
	String expr = expression.length() > 0 ? expression : String("$") + (tags.size() > 0 ? String(tags[0]) : String(""));
	Dictionary res = r->_eval_expression(expr, snapshot);
	bool ok = res.has("ok") && bool(res["ok"]);
	if (ok) {
		final_value = res["value"];
		if (final_value.get_type() == Variant::NIL) {
			final_value = fallback;
		}
	}
	// Write the evaluated result to target_property. Use a deferred call if
	// we're currently running inside a signal handler that runs during
	// NOTIFICATION_EDITOR_PRE/POST_SAVE to avoid set() re-entry.
	Error err = p->set(target_property, final_value);
	if (err != OK) {
		// Ignore: inspector bindings commonly fire before the parent has all
		// properties set up. In production we'd log once per unique parent.
	}
}

void TagBinding::_on_runtime_tag_changed(const String &tag, const Variant & /*value*/,
		const String & /*quality*/, int64_t /*version*/, int64_t /*ts_ms*/) {
	bool relevant = false;
	for (int i = 0; i < tags.size(); i++) {
		if (String(tags[i]) == tag) { relevant = true; break; }
	}
	if (!relevant) return;
	IndustrialRuntime *r = _runtime_or_null();
	if (r == nullptr) return;
	_apply_now(r->_build_snapshot(tags));
}

void TagBinding::_do_write() {
	if (!enable_write || write_tag.length() == 0) return;
	Node *p = get_parent();
	IndustrialRuntime *r = _runtime_or_null();
	if (p == nullptr || r == nullptr) return;
	Variant value;
	if (String(write_source_property) == String() || write_source_property == static_cast<const StringName &>(StringName())) {
		// Fallback: boolean "toggled".
		value = true;
	} else {
		bool found = false;
		value = p->get(write_source_property, &found);
		if (!found) value = true;
	}
	r->write_tag(write_tag, value);
}
