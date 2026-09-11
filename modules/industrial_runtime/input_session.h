#pragma once

#include "tag_keypad_buffer.h"

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"

class InputSession : public RefCounted {
	GDCLASS(InputSession, RefCounted);

protected:
	static void _bind_methods();

public:
	void configure(const Dictionary &p_descriptor);
	void set_validation_callback(const Callable &p_callback) { validation_callback = p_callback; }
	void set_commit_callback(const Callable &p_callback) { commit_callback = p_callback; }

	int64_t get_session_id() const { return session_id; }
	String get_input_mode() const { return input_mode; }
	String get_presentation_mode() const { return presentation_mode; }
	String get_keypad_id() const { return keypad_id; }
	String get_previous_value() const { return previous_value; }
	String get_range_hint() const;
	String get_min_display_text() const;
	String get_max_display_text() const;
	String get_out_of_range_message() const;
	bool should_show_previous_value() const;
	bool should_show_min_value() const;
	bool should_show_max_value() const;
	String get_buffer_text() const;
	String get_display_text() const;
	int get_caret_position() const;
	bool is_modified() const { return modified; }
	bool is_active() const { return active; }

	bool dispatch_edit_action(const String &p_action_id, const Variant &p_payload);
	Dictionary validate_buffer() const;
	Dictionary commit_buffer();
	void cancel();

private:
	int64_t session_id = 0;
	String input_mode = "numeric";
	String presentation_mode = "system";
	String keypad_id;
	String previous_value;
	Dictionary format_config;
	bool mask_display = false;
	bool modified = false;
	bool active = false;
	bool shift_on = false;
	bool has_min = false;
	bool has_max = false;
	double min_value = 0.0;
	double max_value = 0.0;
	bool show_previous_value = true;
	bool show_limits_on_keypad = true;
	String out_of_range_message = "out of range";

	TagKeypadBuffer numeric_buffer;
	String text_buffer;
	int text_caret = 0;

	Callable validation_callback;
	Callable commit_callback;

	bool _dispatch_numeric_action(const String &p_action_id, const Variant &p_payload);
	bool _dispatch_text_action(const String &p_action_id, const Variant &p_payload);
	bool _insert_text(const String &p_text);
	Dictionary _normalize_callback_result(const Variant &p_result, const String &p_default_error) const;
	static String _format_range_value(double p_value);
	String _format_out_of_range_message() const;
	static bool _payload_is_empty(const Variant &p_payload);
	static bool _payload_text(const Variant &p_payload, String &r_text);
	static bool _payload_step(const Variant &p_payload, double &r_step);
	static bool _is_printable_ascii(const String &p_text);
};
