#pragma once

#include "scene/gui/control.h"

#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"

/// TagMeter - engine-native gauge widget (240deg arc + needle).
/// Subscribes a tag through Runtime, maps values onto an angle via the
/// C++ WidgetMeter core, and draws the dial/ticks/needle in _draw().
/// C++ port of native Tag widget.
class TagMeter : public Control {
	GDCLASS(TagMeter, Control);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	TagMeter();

	void set_tag_name(const String &p_tag);
	String get_tag_name() const { return tag_name; }

	void set_min(float p_min) { min_value = p_min; queue_redraw(); }
	float get_min() const { return min_value; }
	void set_max(float p_max) { max_value = p_max; queue_redraw(); }
	float get_max() const { return max_value; }
	void set_red_zone(float p_rz) { red_zone = p_rz; queue_redraw(); }
	float get_red_zone() const { return red_zone; }
	void set_major_ticks(int p_n) { major_ticks = MAX(1, p_n); queue_redraw(); }
	int get_major_ticks() const { return major_ticks; }
	void set_minor_ticks_per_major(int p_n) { minor_ticks_per_major = MAX(0, p_n); queue_redraw(); }
	int get_minor_ticks_per_major() const { return minor_ticks_per_major; }

	void set_arc_color(const Color &c) { arc_color = c; queue_redraw(); }
	Color get_arc_color() const { return arc_color; }
	void set_red_color(const Color &c) { red_color = c; queue_redraw(); }
	Color get_red_color() const { return red_color; }
	void set_needle_color(const Color &c) { needle_color = c; queue_redraw(); }
	Color get_needle_color() const { return needle_color; }
	void set_face_color(const Color &c) { face_color = c; queue_redraw(); }
	Color get_face_color() const { return face_color; }
	void set_tick_color(const Color &c) { tick_color = c; queue_redraw(); }
	Color get_tick_color() const { return tick_color; }

	float get_current_value() const { return current_value; }
	float get_value_angle() const { return value_angle; }
	String get_quality() const { return quality; }

	/// Map a value onto an angle (reuses the C++ WidgetMeter core when present).
	float angle_for_value(float v, float v_min, float v_max) const;

	/// Public entry point used by the bridge callback.
	void apply_external_value(const Variant &p_value);
	void _on_tag_changed(const String &tag, const Variant &v, const String &quality_s, int version, int ts_ms);

private:
	String tag_name;
	float min_value = 0.0f;
	float max_value = 100.0f;
	float red_zone = 80.0f;
	int major_ticks = 5;
	int minor_ticks_per_major = 4;

	Color arc_color = Color(0.25f, 0.55f, 0.95f);
	Color red_color = Color(0.9f, 0.2f, 0.2f);
	Color needle_color = Color(0.95f, 0.95f, 0.98f);
	Color face_color = Color(0.09f, 0.1f, 0.13f);
	Color tick_color = Color(0.8f, 0.82f, 0.88f);

	float current_value = 0.0f;
	float value_angle = -120.0f;
	String quality = "stale";

	static constexpr float DEFAULT_ANGLE_START = -120.0f;
	static constexpr float DEFAULT_ANGLE_END = 120.0f;

	void draw_ticks(const Vector2 &c, float radius);
	void draw_needle(const Vector2 &c, float radius);
	void draw_gauge();
};