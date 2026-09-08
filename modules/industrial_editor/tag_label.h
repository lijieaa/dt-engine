#pragma once

#include "scene/gui/label.h"

#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"

/// TagLabel â€?engine-native label widget that displays a subscribed tag value,
/// tinted by data quality (good/uncertain/bad/stale). C++ port of
/// native Tag widget.
class TagLabel : public Label {
	GDCLASS(TagLabel, Label);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	TagLabel() = default;

	void set_tag_name(const String &p_tag);
	String get_tag_name() const { return tag_name; }

	Color get_good_color() const { return good_color; }
	void set_good_color(const Color &c) { good_color = c; }
	Color get_uncertain_color() const { return uncertain_color; }
	void set_uncertain_color(const Color &c) { uncertain_color = c; }
	Color get_bad_color() const { return bad_color; }
	void set_bad_color(const Color &c) { bad_color = c; }
	Color get_stale_color() const { return stale_color; }
	void set_stale_color(const Color &c) { stale_color = c; }
	Color get_base_color() const { return base_color; }
	void set_base_color(const Color &c) { base_color = c; }

	void set_quality(const String &q);

	void _on_tag_changed(const String &tag, const Variant &v, const String &quality, int version, int ts_ms);

private:
	String tag_name;
	Color base_color = Color(1, 1, 1, 1);
	Color good_color = Color(0.0f, 0.85f, 0.45f, 1.0f);
	Color uncertain_color = Color(1.0f, 0.73f, 0.12f, 1.0f);
	Color bad_color = Color(0.95f, 0.2f, 0.2f, 1.0f);
	Color stale_color = Color(0.65f, 0.65f, 0.65f, 1.0f);
};