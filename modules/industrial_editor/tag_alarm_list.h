#pragma once

#include "scene/gui/item_list.h"

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/variant.h"

class WidgetAlarm;

/// TagAlarmList â€?alarm list widget. Monitors one or more tags via
/// Runtime; each tag_changed is fed to the C++ WidgetAlarm core for
/// threshold evaluation (with debounce), and rows are tinted by active rules.
/// C++ port of native Tag widget.
class TagAlarmList : public ItemList {
	GDCLASS(TagAlarmList, ItemList);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	TagAlarmList();
	~TagAlarmList();

	void set_monitored_tags(const Array &p_tags);
	Array get_monitored_tags() const { return monitored_tags; }

	void set_threshold(float p_th) { threshold = p_th; reconfigure_rules(); }
	float get_threshold() const { return threshold; }
	void set_comparator(const String &p_op) { comparator = p_op; reconfigure_rules(); }
	String get_comparator() const { return comparator; }
	void set_debounce_ms(int p_ms) { debounce_ms = p_ms; reconfigure_rules(); }
	int get_debounce_ms() const { return debounce_ms; }
	void set_active_color(const Color &c) { active_color = c; refresh_rows(); }
	Color get_active_color() const { return active_color; }
	void set_normal_color(const Color &c) { normal_color = c; refresh_rows(); }
	Color get_normal_color() const { return normal_color; }

	int get_active_count() const { return active_count; }

	/// Feed a value for a monitored tag through the alarm core.
	void apply_external_value(const String &p_tag, const Variant &p_value);
	void _on_tag_changed(const String &tag, const Variant &v, const String &quality, int version, int ts_ms);

	/// Rebuild the row list with current active-rule states.
	void refresh_rows();

	/// (Re)register the per-tag threshold rules on the alarm core.
	void reconfigure_rules();

private:
	Array monitored_tags;
	float threshold = 100.0f;
	String comparator = "gt";
	int debounce_ms = 0;
	Color active_color = Color(0.95f, 0.2f, 0.2f);
	Color normal_color = Color(0.85f, 0.87f, 0.92f);

	int active_count = 0;
	Ref<WidgetAlarm> alarm_core;
};