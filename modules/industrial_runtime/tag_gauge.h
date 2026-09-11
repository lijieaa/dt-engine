#pragma once

#include "scene/gui/progress_bar.h"

#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"

/// TagGauge â€?progress gauge (ProgressBar), tinted by low/high warn thresholds.
/// Subscribes a tag through Runtime; value maps onto 0..100% fill and the
/// fill stylebox switches color by percentage band.
/// C++ port of native Tag widget.
class TagGauge : public ProgressBar {
	GDCLASS(TagGauge, ProgressBar);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	TagGauge() = default;

	void set_tag_name(const String &p_tag);
	String get_tag_name() const { return tag_name; }

	void set_range_min(float p) { range_min = p; _tint_for_current(); }
	float get_range_min() const { return range_min; }
	void set_range_max(float p) { range_max = p; _tint_for_current(); }
	float get_range_max() const { return range_max; }
	void set_low_warn_pct(float p) { low_warn_pct = p; _tint_for_current(); }
	float get_low_warn_pct() const { return low_warn_pct; }
	void set_high_warn_pct(float p) { high_warn_pct = p; _tint_for_current(); }
	float get_high_warn_pct() const { return high_warn_pct; }

	/// Public entry point; computes 0..100% and applies tint + tooltip.
	void apply_external_value(const Variant &p_value);
	void _on_tag_changed(const String &tag, const Variant &v, const String &quality, int version, int ts_ms);

private:
	String tag_name;
	float range_min = 0.0f;
	float range_max = 100.0f;
	float low_warn_pct = 15.0f;
	float high_warn_pct = 85.0f;
	float current_pct = 0.0f;

	void _tint_for_current();
};