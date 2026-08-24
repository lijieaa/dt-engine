#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"

#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class WidgetAlarm : public RefCounted {
	GDCLASS(WidgetAlarm, RefCounted);

protected:
	static void _bind_methods();

public:
	String get_version() const;

	/// Register a condition rule.
	/// p_condition keys: "type" (String: "gt","lt","eq","ne","gte","lte"),
	///                   "threshold" (Variant), "debounce_ms" (int, default 0).
	void add_rule(const String &p_rule_id, const Dictionary &p_condition);

	/// Evaluate a value against the named rule. Returns true when the alarm
	/// condition is currently triggered (after debounce elapses). Rules are
	/// evaluated independently; repeated evaluations keep the rule active
	/// while the condition holds (new active-rule additions are idempotent).
	bool evaluate(const String &p_rule_id, const Variant &p_value);

	/// Array of active rule IDs (String), in trigger order.
	Array get_active_rules() const;

	void enable_rule(const String &p_rule_id);
	void disable_rule(const String &p_rule_id);
	void remove_rule(const String &p_rule_id);
	void clear();

private:
	struct Rule {
		String id;
		String type;
		Variant threshold;
		int debounce_ms = 0;
		bool enabled = true;
		uint64_t cond_satisfied_since_ms = 0; // monotonic ticks since condition held
		bool active_due = false;              // condition held + debounce elapsed
	};

	HashMap<String, Rule> rules;
	Vector<String> active_rules; // ordered unique rule IDs currently active

	void set_active(const String &p_rule_id);
	void set_inactive(const String &p_rule_id);
	static bool compare_values(const Variant &p_value, const Variant &p_threshold, const String &p_op);
};