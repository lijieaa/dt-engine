#include "widget_alarm.h"

#include "core/object/class_db.h"
#include "core/os/os.h"

void WidgetAlarm::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_version"), &WidgetAlarm::get_version);

	ClassDB::bind_method(D_METHOD("add_rule", "rule_id", "condition"), &WidgetAlarm::add_rule);
	ClassDB::bind_method(D_METHOD("evaluate", "rule_id", "value"), &WidgetAlarm::evaluate);
	ClassDB::bind_method(D_METHOD("get_active_rules"), &WidgetAlarm::get_active_rules);
	ClassDB::bind_method(D_METHOD("enable_rule", "rule_id"), &WidgetAlarm::enable_rule);
	ClassDB::bind_method(D_METHOD("disable_rule", "rule_id"), &WidgetAlarm::disable_rule);
	ClassDB::bind_method(D_METHOD("remove_rule", "rule_id"), &WidgetAlarm::remove_rule);
	ClassDB::bind_method(D_METHOD("clear"), &WidgetAlarm::clear);
}

String WidgetAlarm::get_version() const {
	return "0.1.0";
}

void WidgetAlarm::add_rule(const String &p_rule_id, const Dictionary &p_condition) {
	Rule r;
	r.id = p_rule_id;

	if (p_condition.has("type")) {
		r.type = (String)p_condition["type"];
	}
	r.type = r.type.strip_edges().to_lower();

	if (p_condition.has("threshold")) {
		r.threshold = p_condition["threshold"];
	}

	r.debounce_ms = 0;
	if (p_condition.has("debounce_ms")) {
		r.debounce_ms = (int)p_condition["debounce_ms"];
	}
	if (r.debounce_ms < 0) {
		r.debounce_ms = 0;
	}

	r.enabled = true;
	r.cond_satisfied_since_ms = 0;
	r.active_due = false;

	rules[p_rule_id] = r;

	// Re-add the rule to the ordered active list (keeps first-trigger order,
	// without weight bookkeeping).
	for (int i = 0; i < active_rules.size(); i++) {
		if (active_rules[i] == p_rule_id) {
			return;
		}
	}
	active_rules.push_back(p_rule_id);
}

bool WidgetAlarm::evaluate(const String &p_rule_id, const Variant &p_value) {
	if (!rules.has(p_rule_id)) {
		return false; // Unknown rule → not triggered.
	}
	Rule &r = rules[p_rule_id];
	if (!r.enabled) {
		r.cond_satisfied_since_ms = 0;
		r.active_due = false;
		set_inactive(p_rule_id);
		return false;
	}

	const bool met = compare_values(p_value, r.threshold, r.type);
	const uint64_t now = OS::get_singleton()->get_ticks_msec();

	if (met) {
		if (r.cond_satisfied_since_ms == 0) {
			r.cond_satisfied_since_ms = now;
		}
		const bool due = (r.debounce_ms <= 0) || (now - r.cond_satisfied_since_ms >= (uint64_t)r.debounce_ms);
		if (due && !r.active_due) {
			r.active_due = true;
			set_active(p_rule_id);
		}
	} else {
		// Condition no longer holds: reset the debounce timer and clear the alarm.
		r.cond_satisfied_since_ms = 0;
		if (r.active_due) {
			r.active_due = false;
			set_inactive(p_rule_id);
		}
	}

	return r.active_due;
}

Array WidgetAlarm::get_active_rules() const {
	Array out;
	for (int i = 0; i < active_rules.size(); i++) {
		const String &rid = active_rules[i];
		const HashMap<String, Rule>::ConstIterator it = rules.find(rid);
		if (it && it->value.active_due) {
			out.push_back(rid);
		}
	}
	return out;
}

void WidgetAlarm::enable_rule(const String &p_rule_id) {
	if (!rules.has(p_rule_id)) {
		return;
	}
	Rule &r = rules[p_rule_id];
	if (!r.enabled) {
		r.enabled = true;
		r.cond_satisfied_since_ms = 0; // fresh debounce window
		r.active_due = false;
	}
}

void WidgetAlarm::disable_rule(const String &p_rule_id) {
	if (!rules.has(p_rule_id)) {
		return;
	}
	Rule &r = rules[p_rule_id];
	r.enabled = false;
	r.cond_satisfied_since_ms = 0;
	r.active_due = false;
	set_inactive(p_rule_id);
}

void WidgetAlarm::remove_rule(const String &p_rule_id) {
	if (!rules.has(p_rule_id)) {
		return;
	}
	rules.erase(p_rule_id);
	set_inactive(p_rule_id);
}

void WidgetAlarm::clear() {
	rules.clear();
	active_rules.clear();
}

void WidgetAlarm::set_active(const String &p_rule_id) {
	for (int i = 0; i < active_rules.size(); i++) {
		if (active_rules[i] == p_rule_id) {
			return; // already present
		}
	}
	active_rules.push_back(p_rule_id);
}

void WidgetAlarm::set_inactive(const String &p_rule_id) {
	for (int i = 0; i < active_rules.size(); i++) {
		if (active_rules[i] == p_rule_id) {
			active_rules.remove_at(i);
			return;
		}
	}
}

bool WidgetAlarm::compare_values(const Variant &p_value, const Variant &p_threshold, const String &p_op) {
	if (p_op == "gt") {
		return Variant::evaluate(Variant::OP_GREATER, p_value, p_threshold);
	}
	if (p_op == "lt") {
		return Variant::evaluate(Variant::OP_LESS, p_value, p_threshold);
	}
	if (p_op == "gte") {
		return Variant::evaluate(Variant::OP_GREATER_EQUAL, p_value, p_threshold);
	}
	if (p_op == "lte") {
		return Variant::evaluate(Variant::OP_LESS_EQUAL, p_value, p_threshold);
	}
	if (p_op == "eq") {
		return Variant::evaluate(Variant::OP_EQUAL, p_value, p_threshold);
	}
	if (p_op == "ne") {
		return Variant::evaluate(Variant::OP_NOT_EQUAL, p_value, p_threshold);
	}
	return false; // unknown comparator
}