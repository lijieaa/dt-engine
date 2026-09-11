#pragma once

#include "scene/gui/line_edit.h"

#include "core/input/input_event.h"
#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include "scene/resources/packed_scene.h"

class TagNumInput : public LineEdit {
	GDCLASS(TagNumInput, LineEdit);

protected:
	static void _bind_methods();
	void _notification(int p_what);
	virtual void gui_input(const Ref<InputEvent> &p_event) override;

public:
	TagNumInput() = default;

	void set_tag_name(const String &p_tag);
	String get_tag_name() const { return tag_name; }
	void set_format_cfg(const Dictionary &p_cfg) { format_cfg = p_cfg; }
	Dictionary get_format_cfg() const { return format_cfg; }

	void set_keypad_id(const String &p_v) { keypad_id = p_v; }
	String get_keypad_id() const { return keypad_id; }
	void set_keypad_scene_override(const Ref<PackedScene> &p_v) { keypad_scene_override = p_v; }
	Ref<PackedScene> get_keypad_scene_override() const { return keypad_scene_override; }
	void set_presentation_mode(const String &p_v) { presentation_mode = p_v; }
	String get_presentation_mode() const { return presentation_mode; }

	void set_use_min(bool p_v) { use_min = p_v; }
	bool get_use_min() const { return use_min; }
	void set_min_value(double p_v) { min_value = p_v; }
	double get_min_value() const { return min_value; }
	void set_use_max(bool p_v) { use_max = p_v; }
	bool get_use_max() const { return use_max; }
	void set_max_value(double p_v) { max_value = p_v; }
	double get_max_value() const { return max_value; }
	void set_show_limits_on_keypad(bool p_v) { show_limits_on_keypad = p_v; }
	bool get_show_limits_on_keypad() const { return show_limits_on_keypad; }
	void set_restart_on_out_of_range(bool p_v) { restart_on_out_of_range = p_v; }
	bool get_restart_on_out_of_range() const { return restart_on_out_of_range; }
	void set_out_of_range_message(const String &p_v) { out_of_range_message = p_v; }
	String get_out_of_range_message() const { return out_of_range_message; }
	void set_show_previous_value(bool p_v) { show_previous_value = p_v; }
	bool get_show_previous_value() const { return show_previous_value; }
	void set_hide_keypad_title(bool p_v) { hide_keypad_title = p_v; }
	bool get_hide_keypad_title() const { return hide_keypad_title; }
	void set_keypad_anchor(const String &p_v) { keypad_anchor = p_v; }
	String get_keypad_anchor() const { return keypad_anchor; }
	void set_keypad_screen_cell(int p_v) { keypad_screen_cell = p_v; }
	int get_keypad_screen_cell() const { return keypad_screen_cell; }
	void set_keypad_side(const String &p_v) { keypad_side = p_v; }
	String get_keypad_side() const { return keypad_side; }
	void set_keypad_align(const String &p_v) { keypad_align = p_v; }
	String get_keypad_align() const { return keypad_align; }

	Dictionary validate_input(const String &p_text);
	Dictionary _validate_input_session_text(const String &p_text) const;
	Dictionary _commit_input_session_text(const String &p_text);
	void _cancel_input_session();
	void _begin_input_session();
	void _on_tag_changed(const String &tag, const Variant &v, const String &quality, int version, int ts_ms);

private:
	String tag_name;
	Dictionary format_cfg;
	Ref<PackedScene> keypad_scene_override;
	String keypad_id;
	String presentation_mode = "system";

	bool use_min = true;
	double min_value = 0.0;
	bool use_max = true;
	double max_value = 100.0;
	bool show_limits_on_keypad = true;
	bool restart_on_out_of_range = false;
	String out_of_range_message = "out of range";
	bool show_previous_value = true;
	bool hide_keypad_title = false;
	String keypad_anchor = "center";
	int keypad_screen_cell = 4;
	String keypad_side = "bottom";
	String keypad_align = "center";

	String _format_out_of_range_message() const;
	void flash_error(const String &p_msg);
	void revert_error_style();
};
