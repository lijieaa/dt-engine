#include "widget_macro.h"

#include "core/object/class_db.h"

void WidgetMacro::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_version"), &WidgetMacro::get_version);
	ClassDB::bind_method(D_METHOD("configure", "cfg"), &WidgetMacro::configure);
	ClassDB::bind_method(D_METHOD("get_steps"), &WidgetMacro::get_steps);
	ClassDB::bind_method(D_METHOD("execute", "immediate"), &WidgetMacro::execute);
	ClassDB::bind_method(D_METHOD("clear"), &WidgetMacro::clear);
	ClassDB::bind_method(D_METHOD("tick_run"), &WidgetMacro::tick_run);
	ClassDB::bind_method(D_METHOD("is_running"), &WidgetMacro::is_running);

	ADD_SIGNAL(MethodInfo("step_done", PropertyInfo(Variant::INT, "idx"),
			PropertyInfo(Variant::STRING, "tag"), PropertyInfo(Variant::NIL, "value")));
	ADD_SIGNAL(MethodInfo("finished", PropertyInfo(Variant::BOOL, "ok")));
}

String WidgetMacro::get_version() const {
	return "0.1.0";
}

bool WidgetMacro::configure(const Dictionary &p_cfg) {
	steps.clear();
	if (!p_cfg.has("steps")) {
		return false;
	}
	const Variant steps_v = p_cfg["steps"];
	if (steps_v.get_type() != Variant::ARRAY) {
		return false;
	}
	Array raw_steps = steps_v;
	for (int i = 0; i < raw_steps.size(); i++) {
		const Variant s = raw_steps[i];
		if (s.get_type() != Variant::DICTIONARY) {
			steps.clear();
			return false;
		}
		Dictionary d = s;
		if (!d.has("tag")) {
			steps.clear();
			return false;
		}
		Step step;
		step.tag = String(d["tag"]);
		step.value = d.has("value") ? d["value"] : Variant();
		step.delay_ms = d.has("delay_ms") ? (int)d["delay_ms"] : 0;
		steps.push_back(step);
	}
	return true;
}

Array WidgetMacro::get_steps() const {
	Array result;
	for (int i = 0; i < steps.size(); i++) {
		Dictionary d;
		d["tag"] = steps[i].tag;
		d["value"] = steps[i].value;
		d["delay_ms"] = steps[i].delay_ms;
		result.push_back(d);
	}
	return result;
}

bool WidgetMacro::execute(bool p_immediate) {
	if (running) {
		return false; // A run is already in progress.
	}
	if (steps.is_empty()) {
		emit_signal(SNAME("finished"), true); // Empty macro: trivially finished.
		return true;
	}
	if (p_immediate) {
		run_all();
		return true;
	}
	// Deferred: schedule for next frame (tick_run is called by the host).
	scheduled = true;
	return true;
}

void WidgetMacro::tick_run() {
	if (scheduled) {
		scheduled = false;
		run_all();
	}
}

bool WidgetMacro::is_running() const {
	return running || scheduled;
}

void WidgetMacro::run_all() {
	running = true;
	for (int i = 0; i < steps.size(); i++) {
		emit_signal(SNAME("step_done"), i, steps[i].tag, steps[i].value);
	}
	running = false;
	emit_signal(SNAME("finished"), true);
}

void WidgetMacro::clear() {
	steps.clear();
	scheduled = false;
	running = false;
}