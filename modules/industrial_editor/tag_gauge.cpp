#include "tag_gauge.h"

#include "tag_widget_util.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/variant/callable.h"
#include "scene/main/node.h"
#include "scene/resources/style_box_flat.h"

void TagGauge::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tag_name", "tag"), &TagGauge::set_tag_name);
	ClassDB::bind_method(D_METHOD("get_tag_name"), &TagGauge::get_tag_name);
	ClassDB::bind_method(D_METHOD("set_range_min", "v"), &TagGauge::set_range_min);
	ClassDB::bind_method(D_METHOD("get_range_min"), &TagGauge::get_range_min);
	ClassDB::bind_method(D_METHOD("set_range_max", "v"), &TagGauge::set_range_max);
	ClassDB::bind_method(D_METHOD("get_range_max"), &TagGauge::get_range_max);
	ClassDB::bind_method(D_METHOD("set_low_warn_pct", "v"), &TagGauge::set_low_warn_pct);
	ClassDB::bind_method(D_METHOD("get_low_warn_pct"), &TagGauge::get_low_warn_pct);
	ClassDB::bind_method(D_METHOD("set_high_warn_pct", "v"), &TagGauge::set_high_warn_pct);
	ClassDB::bind_method(D_METHOD("get_high_warn_pct"), &TagGauge::get_high_warn_pct);
	ClassDB::bind_method(D_METHOD("apply_external_value", "value"), &TagGauge::apply_external_value);
	ClassDB::bind_method(D_METHOD("_on_tag_changed", "tag", "v", "quality", "version", "ts_ms"), &TagGauge::_on_tag_changed);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tag_name"), "set_tag_name", "get_tag_name");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "range_min"), "set_range_min", "get_range_min");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "range_max"), "set_range_max", "get_range_max");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "low_warn_pct"), "set_low_warn_pct", "get_low_warn_pct");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "high_warn_pct"), "set_high_warn_pct", "get_high_warn_pct");
}

void TagGauge::set_tag_name(const String &p_tag) {
	tag_name = p_tag;
	if (tag_name.is_empty()) {
		return;
	}
	tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagGauge::_on_tag_changed));
}

void TagGauge::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		if (!tag_name.is_empty()) {
			tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagGauge::_on_tag_changed));
		}
		_tint_for_current();
	}
}

void TagGauge::_on_tag_changed(const String &tag, const Variant &v, const String &quality, int version, int ts_ms) {
	if (tag != tag_name) {
		return;
	}
	apply_external_value(v);
	set_tooltip_text(tag_name + " = " + v.operator String() + " [" + quality + "]");
}

void TagGauge::apply_external_value(const Variant &p_value) {
	if (p_value.get_type() == Variant::INT || p_value.get_type() == Variant::FLOAT) {
		const double fv = double(p_value);
		const double span = MAX(0.0001, double(range_max) - double(range_min));
		const float pct = CLAMP((float)((fv - range_min) / span * 100.0), 0.0f, 100.0f);
		set_value(pct);
		_tint_for_current();
		set_tooltip_text(String::num(fv) + " / [" + String::num(range_min) + ", " + String::num(range_max) + "]");
	}
}

void TagGauge::_tint_for_current() {
	current_pct = (float)get_value();
	Ref<StyleBoxFlat> sb;
	sb.instantiate();
	if (current_pct < low_warn_pct) {
		sb->set_bg_color(Color(1.0f, 0.7f, 0.1f, 0.9f));
	} else if (current_pct > high_warn_pct) {
		sb->set_bg_color(Color(0.98f, 0.2f, 0.25f, 0.9f));
	} else {
		sb->set_bg_color(Color(0.18f, 0.78f, 0.95f, 0.95f));
	}
	sb->set_corner_radius_all(8);
	add_theme_style_override("fill", sb);
}