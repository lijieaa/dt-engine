#pragma once

#include "scene/gui/panel.h"

#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

/// TagLamp - panel indicator that lights when its subscribed tag is truthy and
/// optionally blinks while lit.
class TagLamp : public Panel {
	GDCLASS(TagLamp, Panel);

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void _process(double p_delta);

public:
	TagLamp();

	void set_tag_name(const String &p_tag);
	String get_tag_name() const { return tag_name; }
	void set_on_color(const Color &c) { on_color = c; refresh(); }
	Color get_on_color() const { return on_color; }
	void set_off_color(const Color &c) { off_color = c; refresh(); }
	Color get_off_color() const { return off_color; }
	void set_base_bg(const Color &c) { base_bg = c; }
	Color get_base_bg() const { return base_bg; }
	void set_blink_cfg(const Dictionary &p_cfg) { blink_cfg = p_cfg; }
	Dictionary get_blink_cfg() const { return blink_cfg; }

	bool is_lit() const { return lit; }
	void apply_external_value(const Variant &p_value);
	void _on_tag_changed(const String &tag, const Variant &v, const String &quality, int version, int ts_ms);

private:
	String tag_name;
	Color on_color = Color(0.95f, 0.15f, 0.15f);
	Color off_color = Color(0.15f, 0.15f, 0.18f);
	Color base_bg = Color(0.07f, 0.08f, 0.1f);
	Dictionary blink_cfg;
	bool lit = false;
	double blink_phase = 0.0;
	Color target_color;

	void refresh();
	void toggle_blink();
};