#include "tag_bar.h"

#include "tag_widget_util.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/variant/callable.h"
#include "scene/main/node.h"
#include "scene/resources/style_box_flat.h"

void TagBar::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tag_name", "tag"), &TagBar::set_tag_name);
	ClassDB::bind_method(D_METHOD("get_tag_name"), &TagBar::get_tag_name);
	ClassDB::bind_method(D_METHOD("set_bar_tint", "c"), &TagBar::set_bar_tint);
	ClassDB::bind_method(D_METHOD("get_bar_tint"), &TagBar::get_bar_tint);
	ClassDB::bind_method(D_METHOD("set_warn_tint", "c"), &TagBar::set_warn_tint);
	ClassDB::bind_method(D_METHOD("get_warn_tint"), &TagBar::get_warn_tint);
	ClassDB::bind_method(D_METHOD("set_danger_tint", "c"), &TagBar::set_danger_tint);
	ClassDB::bind_method(D_METHOD("get_danger_tint"), &TagBar::get_danger_tint);
	ClassDB::bind_method(D_METHOD("set_warn_pct", "p"), &TagBar::set_warn_pct);
	ClassDB::bind_method(D_METHOD("get_warn_pct"), &TagBar::get_warn_pct);
	ClassDB::bind_method(D_METHOD("set_danger_pct", "p"), &TagBar::set_danger_pct);
	ClassDB::bind_method(D_METHOD("get_danger_pct"), &TagBar::get_danger_pct);
	ClassDB::bind_method(D_METHOD("apply_external_value", "value"), &TagBar::apply_external_value);
	ClassDB::bind_method(D_METHOD("_on_tag_changed", "tag", "v", "quality", "version", "ts_ms"), &TagBar::_on_tag_changed);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tag_name"), "set_tag_name", "get_tag_name");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "bar_tint"), "set_bar_tint", "get_bar_tint");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "warn_tint"), "set_warn_tint", "get_warn_tint");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "danger_tint"), "set_danger_tint", "get_danger_tint");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "warn_pct"), "set_warn_pct", "get_warn_pct");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "danger_pct"), "set_danger_pct", "get_danger_pct");
}

void TagBar::set_tag_name(const String &p_tag) {
	tag_name = p_tag;
	if (tag_name.is_empty()) {
		return;
	}
	tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagBar::_on_tag_changed));
}

void TagBar::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		set_mouse_filter(MOUSE_FILTER_IGNORE);
		if (!tag_name.is_empty()) {
			tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagBar::_on_tag_changed));
		}
	}
}

void TagBar::_on_tag_changed(const String &tag, const Variant &v, const String &quality, int version, int ts_ms) {
	if (tag != tag_name) {
		return;
	}
	apply_external_value(v);
	set_tooltip_text(tag_name + " = " + v.operator String() + " [" + quality + "]");
}

void TagBar::apply_external_value(const Variant &p_value) {
	if (p_value.get_type() == Variant::INT || p_value.get_type() == Variant::FLOAT) {
		const double f = CLAMP(double(p_value), get_min(), get_max());
		set_value(f);
		const double span = get_max() - get_min();
		const float pct = (span <= 0.0) ? 0.0f : (float)((f - get_min()) / span * 100.0);
		_apply_fill_pct(pct);
	}
}

void TagBar::_apply_fill_pct(float pct) {
	current_pct = pct;
	Ref<StyleBoxFlat> sb;
	sb.instantiate();
	Color c = bar_tint;
	if (pct >= danger_pct) {
		c = danger_tint;
	} else if (pct >= warn_pct) {
		c = warn_tint;
	}
	sb->set_bg_color(c);
	sb->set_corner_radius_all(6);
	add_theme_style_override("slider", sb);
}