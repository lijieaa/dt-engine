#include "widget_meter.h"

#include "core/math/math_defs.h"
#include "core/object/class_db.h"
#include "core/variant/dictionary.h"

void WidgetMeter::_bind_methods() {
	ClassDB::bind_static_method("WidgetMeter", D_METHOD("angle_for_value", "value", "min_v", "max_v", "cfg"), &WidgetMeter::angle_for_value);
	ClassDB::bind_method(D_METHOD("get_version"), &WidgetMeter::get_version);
}

String WidgetMeter::get_version() const {
	return "0.1.0";
}

float WidgetMeter::angle_for_value(float p_value, float p_min_v, float p_max_v, const Dictionary &p_cfg) {
	// Default 240-degree gauge: -120deg .. +120deg.
	float angle_start = -120.0f;
	float angle_end = 120.0f;
	if (p_cfg.has("angle_start")) {
		angle_start = float(p_cfg["angle_start"]);
	}
	if (p_cfg.has("angle_end")) {
		angle_end = float(p_cfg["angle_end"]);
	}
	if (p_max_v <= p_min_v) {
		return angle_start; // Degenerate range: point at start.
	}
	const float clamped = CLAMP(p_value, p_min_v, p_max_v);
	const float ratio = (clamped - p_min_v) / (p_max_v - p_min_v);
	return angle_start + ratio * (angle_end - angle_start);
}