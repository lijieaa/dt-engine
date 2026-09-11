#pragma once

#include "scene/gui/check_box.h"

#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"

/// TagSwitch â€?check box that mirrors a boolean tag and writes user toggles
/// back through Runtime.
class TagSwitch : public CheckBox {
	GDCLASS(TagSwitch, CheckBox);

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void _on_toggled(bool p_pressed);

public:
	TagSwitch() = default;

	void set_tag_name(const String &p_tag);
	String get_tag_name() const { return tag_name; }
	void set_on_color(const Color &c) { on_color = c; }
	Color get_on_color() const { return on_color; }
	void set_off_color(const Color &c) { off_color = c; }
	Color get_off_color() const { return off_color; }

	void apply_external_value(const Variant &p_value);
	void _on_tag_changed(const String &tag, const Variant &v, const String &quality, int version, int ts_ms);

private:
	String tag_name;
	Color on_color = Color(0.1f, 0.9f, 0.5f, 1.0f);
	Color off_color = Color(0.6f, 0.6f, 0.65f, 1.0f);

	void refresh_tint();
};