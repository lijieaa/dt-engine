#pragma once

#include "scene/gui/slider.h"

#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"

/// TagBar â€?horizontal status bar (HSlider, non-interactive).
/// Subscribes a tag through Runtime; value maps onto the slider range and
/// the fill tint switches by warn/danger percentage thresholds.
/// C++ port of native Tag widget.
class TagBar : public HSlider {
	GDCLASS(TagBar, HSlider);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	TagBar() = default;

	void set_tag_name(const String &p_tag);
	String get_tag_name() const { return tag_name; }

	void set_bar_tint(const Color &c) { bar_tint = c; _apply_fill_pct(current_pct); }
	Color get_bar_tint() const { return bar_tint; }
	void set_warn_tint(const Color &c) { warn_tint = c; _apply_fill_pct(current_pct); }
	Color get_warn_tint() const { return warn_tint; }
	void set_danger_tint(const Color &c) { danger_tint = c; _apply_fill_pct(current_pct); }
	Color get_danger_tint() const { return danger_tint; }
	void set_warn_pct(float p) { warn_pct = p; _apply_fill_pct(current_pct); }
	float get_warn_pct() const { return warn_pct; }
	void set_danger_pct(float p) { danger_pct = p; _apply_fill_pct(current_pct); }
	float get_danger_pct() const { return danger_pct; }

	/// Public entry point; clamps to [min, max], updates value + tint.
	void apply_external_value(const Variant &p_value);
	void _on_tag_changed(const String &tag, const Variant &v, const String &quality, int version, int ts_ms);

private:
	String tag_name;
	Color bar_tint = Color(0.25f, 0.75f, 1.0f, 1.0f);
	Color warn_tint = Color(1.0f, 0.7f, 0.1f, 1.0f);
	Color danger_tint = Color(0.97f, 0.22f, 0.22f, 1.0f);
	float warn_pct = 80.0f;
	float danger_pct = 95.0f;
	float current_pct = 0.0f;

	void _apply_fill_pct(float pct);
};