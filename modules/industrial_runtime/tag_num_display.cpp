#include "tag_num_display.h"

#include "tag_widget_util.h"
#include "widget_format.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"

void TagNumDisplay::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tag_name", "tag"), &TagNumDisplay::set_tag_name);
	ClassDB::bind_method(D_METHOD("get_tag_name"), &TagNumDisplay::get_tag_name);
	ClassDB::bind_method(D_METHOD("set_format_cfg", "cfg"), &TagNumDisplay::set_format_cfg);
	ClassDB::bind_method(D_METHOD("get_format_cfg"), &TagNumDisplay::get_format_cfg);
	ClassDB::bind_method(D_METHOD("format_value", "v"), &TagNumDisplay::format_value);
	ClassDB::bind_method(D_METHOD("apply_external_value", "value", "quality"), &TagNumDisplay::apply_external_value, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("_on_tag_changed", "tag", "v", "quality", "version", "ts_ms"), &TagNumDisplay::_on_tag_changed);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tag_name"), "set_tag_name", "get_tag_name");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "format_cfg"), "set_format_cfg", "get_format_cfg");
}

void TagNumDisplay::set_tag_name(const String &p_tag) {
	tag_name = p_tag;
	if (tag_name.is_empty()) {
		return;
	}
	tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagNumDisplay::_on_tag_changed));
}

String TagNumDisplay::format_value(const Variant &v) {
	if (ClassDB::class_exists("WidgetFormat")) {
		const Variant ret = WidgetFormat::format_value(v, format_cfg);
		return ret.operator String();
	}
	return v.operator String();
}

void TagNumDisplay::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		if (get_text().is_empty()) {
			set_text("-");
		}
		if (!tag_name.is_empty()) {
			tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagNumDisplay::_on_tag_changed));
		}
	}
}

void TagNumDisplay::apply_external_value(const Variant &p_value, const String &p_quality) {
	set_text(format_value(p_value));
	queue_redraw();
	Color c(1, 1, 1);
	if (p_quality == "good") {
		c = Color(0.9f, 0.95f, 1.0f);
	} else if (p_quality == "uncertain") {
		c = Color(1.0f, 0.73f, 0.12f);
	} else if (p_quality == "bad") {
		c = Color(0.95f, 0.2f, 0.2f);
	} else if (p_quality == "stale") {
		c = Color(0.6f, 0.6f, 0.6f);
	} else if (p_quality.is_empty()) {
		return;
	}
	add_theme_color_override("font_color", c);
}

void TagNumDisplay::_on_tag_changed(const String &tag, const Variant &v, const String &quality, int64_t version, int64_t ts_ms) {
	if (tag != tag_name) {
		return;
	}
	apply_external_value(v, quality);
}