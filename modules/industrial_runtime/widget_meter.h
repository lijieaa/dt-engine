#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"

class WidgetMeter : public RefCounted {
	GDCLASS(WidgetMeter, RefCounted);

protected:
	static void _bind_methods();

public:
	String get_version() const;

	/// Map a value onto an arc angle (degrees) for a gauge.
	/// 0.0 -> angle_start, max_v -> angle_end (min_v..max_v linear interpolation),
	/// clamped to [min_v, max_v]. Default arc: -120deg .. +120deg (HMI-style
	/// 240-degree gauge) unless overridden via cfg.
	/// cfg optional keys: angle_start:float, angle_end:float (degrees, CCW-positive).
	static float angle_for_value(float p_value, float p_min_v, float p_max_v, const Dictionary &p_cfg = Dictionary());
};