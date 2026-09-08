#include "tag_label.h"

#include "tag_widget_util.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "core/variant/callable.h"

void TagLabel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tag_name", "tag"), &TagLabel::set_tag_name);
	ClassDB::bind_method(D_METHOD("get_tag_name"), &TagLabel::get_tag_name);
	ClassDB::bind_method(D_METHOD("set_base_color", "c"), &TagLabel::set_base_color);
	ClassDB::bind_method(D_METHOD("get_base_color"), &TagLabel::get_base_color);
	ClassDB::bind_method(D_METHOD("set_good_color", "c"), &TagLabel::set_good_color);
	ClassDB::bind_method(D_METHOD("get_good_color"), &TagLabel::get_good_color);
	ClassDB::bind_method(D_METHOD("set_uncertain_color", "c"), &TagLabel::set_uncertain_color);
	ClassDB::bind_method(D_METHOD("get_uncertain_color"), &TagLabel::get_uncertain_color);
	ClassDB::bind_method(D_METHOD("set_bad_color", "c"), &TagLabel::set_bad_color);
	ClassDB::bind_method(D_METHOD("get_bad_color"), &TagLabel::get_bad_color);
	ClassDB::bind_method(D_METHOD("set_stale_color", "c"), &TagLabel::set_stale_color);
	ClassDB::bind_method(D_METHOD("get_stale_color"), &TagLabel::get_stale_color);

	ClassDB::bind_method(D_METHOD("set_quality", "q"), &TagLabel::set_quality);
	ClassDB::bind_method(D_METHOD("_on_tag_changed", "tag", "v", "quality", "version", "ts_ms"), &TagLabel::_on_tag_changed);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tag_name"), "set_tag_name", "get_tag_name");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "base_color"), "set_base_color", "get_base_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "good_color"), "set_good_color", "get_good_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "uncertain_color"), "set_uncertain_color", "get_uncertain_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "bad_color"), "set_bad_color", "get_bad_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "stale_color"), "set_stale_color", "get_stale_color");
}

void TagLabel::set_tag_name(const String &p_tag) {
	tag_name = p_tag;
	if (tag_name.is_empty()) {
		return;
	}
	tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagLabel::_on_tag_changed));
}

void TagLabel::set_quality(const String &q) {
	Color c = base_color;
	if (q == "good") {
		c = good_color;
	} else if (q == "uncertain") {
		c = uncertain_color;
	} else if (q == "bad") {
		c = bad_color;
	} else if (q == "stale") {
		c = stale_color;
	}
	add_theme_color_override("font_color", c);
}

void TagLabel::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		if (get_text().is_empty()) {
			set_text("-");
		}
		if (!tag_name.is_empty()) {
			tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagLabel::_on_tag_changed));
		}
	}
}

void TagLabel::_on_tag_changed(const String &tag, const Variant &v, const String &quality, int version, int ts_ms) {
	if (tag != tag_name) {
		return;
	}
	set_text(v.operator String());
	set_quality(quality);
}