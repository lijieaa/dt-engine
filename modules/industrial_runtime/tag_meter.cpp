#include "tag_meter.h"

#include "tag_widget_util.h"
#include "widget_meter.h"

#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/variant/callable.h"
#include "scene/main/node.h"
#include "scene/scene_string_names.h"

TagMeter::TagMeter() {
	set_custom_minimum_size(Size2(140, 140));
}

void TagMeter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tag_name", "tag"), &TagMeter::set_tag_name);
	ClassDB::bind_method(D_METHOD("get_tag_name"), &TagMeter::get_tag_name);
	ClassDB::bind_method(D_METHOD("set_min", "v"), &TagMeter::set_min);
	ClassDB::bind_method(D_METHOD("get_min"), &TagMeter::get_min);
	ClassDB::bind_method(D_METHOD("set_max", "v"), &TagMeter::set_max);
	ClassDB::bind_method(D_METHOD("get_max"), &TagMeter::get_max);
	ClassDB::bind_method(D_METHOD("set_red_zone", "v"), &TagMeter::set_red_zone);
	ClassDB::bind_method(D_METHOD("get_red_zone"), &TagMeter::get_red_zone);
	ClassDB::bind_method(D_METHOD("set_major_ticks", "n"), &TagMeter::set_major_ticks);
	ClassDB::bind_method(D_METHOD("get_major_ticks"), &TagMeter::get_major_ticks);
	ClassDB::bind_method(D_METHOD("set_minor_ticks_per_major", "n"), &TagMeter::set_minor_ticks_per_major);
	ClassDB::bind_method(D_METHOD("get_minor_ticks_per_major"), &TagMeter::get_minor_ticks_per_major);
	ClassDB::bind_method(D_METHOD("set_arc_color", "c"), &TagMeter::set_arc_color);
	ClassDB::bind_method(D_METHOD("get_arc_color"), &TagMeter::get_arc_color);
	ClassDB::bind_method(D_METHOD("set_red_color", "c"), &TagMeter::set_red_color);
	ClassDB::bind_method(D_METHOD("get_red_color"), &TagMeter::get_red_color);
	ClassDB::bind_method(D_METHOD("set_needle_color", "c"), &TagMeter::set_needle_color);
	ClassDB::bind_method(D_METHOD("get_needle_color"), &TagMeter::get_needle_color);
	ClassDB::bind_method(D_METHOD("set_face_color", "c"), &TagMeter::set_face_color);
	ClassDB::bind_method(D_METHOD("get_face_color"), &TagMeter::get_face_color);
	ClassDB::bind_method(D_METHOD("set_tick_color", "c"), &TagMeter::set_tick_color);
	ClassDB::bind_method(D_METHOD("get_tick_color"), &TagMeter::get_tick_color);
	ClassDB::bind_method(D_METHOD("get_current_value"), &TagMeter::get_current_value);
	ClassDB::bind_method(D_METHOD("get_value_angle"), &TagMeter::get_value_angle);
	ClassDB::bind_method(D_METHOD("get_quality"), &TagMeter::get_quality);
	ClassDB::bind_method(D_METHOD("angle_for_value", "v", "v_min", "v_max"), &TagMeter::angle_for_value);
	ClassDB::bind_method(D_METHOD("apply_external_value", "value"), &TagMeter::apply_external_value);
	ClassDB::bind_method(D_METHOD("_on_tag_changed", "tag", "v", "quality_s", "version", "ts_ms"), &TagMeter::_on_tag_changed);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tag_name"), "set_tag_name", "get_tag_name");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min"), "set_min", "get_min");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max"), "set_max", "get_max");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "red_zone"), "set_red_zone", "get_red_zone");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "major_ticks"), "set_major_ticks", "get_major_ticks");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "minor_ticks_per_major"), "set_minor_ticks_per_major", "get_minor_ticks_per_major");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "arc_color"), "set_arc_color", "get_arc_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "red_color"), "set_red_color", "get_red_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "needle_color"), "set_needle_color", "get_needle_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "face_color"), "set_face_color", "get_face_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "tick_color"), "set_tick_color", "get_tick_color");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "current_value"), "", "get_current_value");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "value_angle"), "", "get_value_angle");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "quality"), "", "get_quality");
}

void TagMeter::set_tag_name(const String &p_tag) {
	tag_name = p_tag;
	if (tag_name.is_empty()) {
		return;
	}
	tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagMeter::_on_tag_changed));
}

void TagMeter::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		if (!tag_name.is_empty()) {
			tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagMeter::_on_tag_changed));
		}
		queue_redraw();
	} else if (p_what == NOTIFICATION_DRAW) {
		draw_gauge();
	}
}

float TagMeter::angle_for_value(float v, float v_min, float v_max) const {
	// Reuse the C++ WidgetMeter core: linear mapping -120..+120 clamped.
	return WidgetMeter::angle_for_value(v, v_min, v_max, Dictionary());
}

void TagMeter::apply_external_value(const Variant &p_value) {
	float f = tag_widget::to_float(p_value, 0.0f);
	current_value = f;
	value_angle = angle_for_value(f, min_value, max_value);
	queue_redraw();
}

void TagMeter::_on_tag_changed(const String &tag, const Variant &v, const String &quality_s, int version, int ts_ms) {
	if (tag != tag_name) {
		return;
	}
	quality = quality_s;
	apply_external_value(v);
}

void TagMeter::draw_gauge() {
	const Vector2 c = get_size() / 2.0f;
	const float radius = MIN(get_size().x, get_size().y) * 0.42f;
	if (radius <= 0.0f) {
		return;
	}
	// Dial face.
	draw_circle(c, radius, face_color);
	// Main arc (240° starting at -120 == "6 o'clock" since -90 offset).
	draw_arc(c, radius, Math::deg_to_rad(DEFAULT_ANGLE_START - 90.0f), Math::deg_to_rad(DEFAULT_ANGLE_END - 90.0f), 96, arc_color, 2.5f, true);
	// Red zone.
	const float red_ratio = CLAMP((red_zone - min_value) / (max_value - min_value), 0.0f, 1.0f);
	const float red_start = DEFAULT_ANGLE_START + red_ratio * (DEFAULT_ANGLE_END - DEFAULT_ANGLE_START);
	draw_arc(c, radius * 0.92f, Math::deg_to_rad(red_start - 90.0f), Math::deg_to_rad(DEFAULT_ANGLE_END - 90.0f), 48, red_color, 3.0f, true);
	draw_ticks(c, radius);
	draw_needle(c, radius);
}

void TagMeter::draw_ticks(const Vector2 &c, float radius) {
	const int total = major_ticks * (minor_ticks_per_major + 1);
	for (int i = 0; i <= total; i++) {
		const float t = float(i) / float(MAX(1, total));
		const float ang = Math::deg_to_rad(DEFAULT_ANGLE_START + t * (DEFAULT_ANGLE_END - DEFAULT_ANGLE_START) - 90.0f);
		const bool is_major = (i % (minor_ticks_per_major + 1)) == 0;
		const float r_in = radius * (is_major ? 0.86f : 0.90f);
		const float r_out = radius * 0.97f;
		const Vector2 dir(Math::cos(ang), Math::sin(ang));
		draw_line(c + dir * r_in, c + dir * r_out, tick_color, is_major ? 2.0f : 1.0f);
	}
}

void TagMeter::draw_needle(const Vector2 &c, float radius) {
	const float ang = Math::deg_to_rad(value_angle - 90.0f);
	const Vector2 dir(Math::cos(ang), Math::sin(ang));
	const Vector2 tip = c + dir * radius * 0.78f;
	draw_line(c, tip, needle_color, 2.0f);
	draw_circle(c, 5.0f, needle_color);
}