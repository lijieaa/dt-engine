#pragma once

#include "scene/gui/label.h"

#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

/// TagNumDisplay - numeric display widget that formats a subscribed tag value
/// through the C++ WidgetFormat core (thousands/decimals/prefix/suffix/base/BCD)
/// and tints by quality. C++ port of tag_num_display.gd.
class TagNumDisplay : public Label {
	GDCLASS(TagNumDisplay, Label);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	TagNumDisplay() = default;

	void set_tag_name(const String &p_tag);
	String get_tag_name() const { return tag_name; }
	void set_format_cfg(const Dictionary &p_cfg) { format_cfg = p_cfg; }
	Dictionary get_format_cfg() const { return format_cfg; }

	String format_value(const Variant &v);
	void apply_external_value(const Variant &p_value, const String &p_quality = String());
	void _on_tag_changed(const String &tag, const Variant &v, const String &quality, int64_t version, int64_t ts_ms);

private:
	String tag_name;
	Dictionary format_cfg;
};