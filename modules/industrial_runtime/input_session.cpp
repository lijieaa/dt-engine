#include "input_session.h"

#include "keypad_action.h"

#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/variant.h"

void InputSession::_bind_methods() {
	ClassDB::bind_method(D_METHOD("configure", "descriptor"), &InputSession::configure);
	ClassDB::bind_method(D_METHOD("set_validation_callback", "callback"), &InputSession::set_validation_callback);
	ClassDB::bind_method(D_METHOD("set_commit_callback", "callback"), &InputSession::set_commit_callback);
	ClassDB::bind_method(D_METHOD("get_session_id"), &InputSession::get_session_id);
	ClassDB::bind_method(D_METHOD("get_input_mode"), &InputSession::get_input_mode);
	ClassDB::bind_method(D_METHOD("get_presentation_mode"), &InputSession::get_presentation_mode);
	ClassDB::bind_method(D_METHOD("get_keypad_id"), &InputSession::get_keypad_id);
	ClassDB::bind_method(D_METHOD("get_previous_value"), &InputSession::get_previous_value);
	ClassDB::bind_method(D_METHOD("get_range_hint"), &InputSession::get_range_hint);
	ClassDB::bind_method(D_METHOD("get_min_display_text"), &InputSession::get_min_display_text);
	ClassDB::bind_method(D_METHOD("get_max_display_text"), &InputSession::get_max_display_text);
	ClassDB::bind_method(D_METHOD("get_out_of_range_message"), &InputSession::get_out_of_range_message);
	ClassDB::bind_method(D_METHOD("get_buffer_text"), &InputSession::get_buffer_text);
	ClassDB::bind_method(D_METHOD("get_display_text"), &InputSession::get_display_text);
	ClassDB::bind_method(D_METHOD("get_caret_position"), &InputSession::get_caret_position);
	ClassDB::bind_method(D_METHOD("is_modified"), &InputSession::is_modified);
	ClassDB::bind_method(D_METHOD("is_active"), &InputSession::is_active);
	ClassDB::bind_method(D_METHOD("dispatch_edit_action", "action_id", "payload"), &InputSession::dispatch_edit_action);
	ClassDB::bind_method(D_METHOD("validate_buffer"), &InputSession::validate_buffer);
	ClassDB::bind_method(D_METHOD("commit_buffer"), &InputSession::commit_buffer);
	ClassDB::bind_method(D_METHOD("cancel"), &InputSession::cancel);
}

void InputSession::configure(const Dictionary &p_descriptor) {
	session_id = (int64_t)p_descriptor.get("session_id", int64_t(0));
	input_mode = p_descriptor.get("input_mode", String("numeric"));
	presentation_mode = p_descriptor.get("presentation_mode", String("system"));
	keypad_id = p_descriptor.get("keypad_id", String());
	previous_value = p_descriptor.get("previous_value", String());
	format_config = p_descriptor.get("format_config", Dictionary());
	mask_display = (bool)p_descriptor.get("mask_display", false) || input_mode == "password";
	has_min = (bool)p_descriptor.get("has_min", false);
	has_max = (bool)p_descriptor.get("has_max", false);
	min_value = (double)p_descriptor.get("min_value", 0.0);
	max_value = (double)p_descriptor.get("max_value", 0.0);
	show_previous_value = (bool)p_descriptor.get("show_previous_value", true);
	show_limits_on_keypad = (bool)p_descriptor.get("show_limits_on_keypad", true);
	out_of_range_message = p_descriptor.get("out_of_range_message", String("out of range"));
	modified = false;
	active = true;
	shift_on = false;

	const String initial_value = p_descriptor.get("initial_value", String());
	if (input_mode == "numeric") {
		numeric_buffer = tag_keypad_buffer_from_initial(initial_value);
		text_buffer = String();
		text_caret = 0;
	} else {
		text_buffer = initial_value;
		text_caret = text_buffer.length();
		numeric_buffer = TagKeypadBuffer();
	}
}

String InputSession::get_buffer_text() const {
	return input_mode == "numeric" ? numeric_buffer.text : text_buffer;
}

String InputSession::get_range_hint() const {
	if (!show_limits_on_keypad || (!has_min && !has_max)) {
		return String();
	}

	const String lower = has_min ? _format_range_value(min_value) : String("...");
	const String upper = has_max ? _format_range_value(max_value) : String("...");
	return lower + " - " + upper;
}

String InputSession::get_min_display_text() const {
	if (!should_show_min_value()) {
		return String();
	}
	return _format_range_value(min_value);
}

String InputSession::get_max_display_text() const {
	if (!should_show_max_value()) {
		return String();
	}
	return _format_range_value(max_value);
}

bool InputSession::should_show_previous_value() const {
	return show_previous_value && !previous_value.is_empty();
}

bool InputSession::should_show_min_value() const {
	return show_limits_on_keypad && has_min;
}

bool InputSession::should_show_max_value() const {
	return show_limits_on_keypad && has_max;
}

String InputSession::get_out_of_range_message() const {
	return _format_out_of_range_message();
}

String InputSession::_format_out_of_range_message() const {
	String message = out_of_range_message.strip_edges();
	if (message.is_empty()) {
		message = "out of range";
	}
	const String min_text = has_min ? _format_range_value(min_value) : String("...");
	const String max_text = has_max ? _format_range_value(max_value) : String("...");
	return message.replace("{min}", min_text).replace("{max}", max_text);
}

String InputSession::_format_range_value(double p_value) {
	if (Math::is_equal_approx(p_value, Math::round(p_value))) {
		return String::num_int64((int64_t)Math::round(p_value));
	}
	return String::num(p_value);
}

String InputSession::get_display_text() const {
	const String value = get_buffer_text();
	if (!mask_display) {
		return value;
	}
	return String("*").repeat(value.length());
}

int InputSession::get_caret_position() const {
	return input_mode == "numeric" ? numeric_buffer.caret : text_caret;
}

bool InputSession::_payload_is_empty(const Variant &p_payload) {
	if (p_payload.get_type() == Variant::NIL) {
		return true;
	}
	return p_payload.get_type() == Variant::DICTIONARY && ((Dictionary)p_payload).is_empty();
}

bool InputSession::_payload_text(const Variant &p_payload, String &r_text) {
	if (p_payload.get_type() == Variant::STRING) {
		r_text = p_payload;
		return true;
	}
	if (p_payload.get_type() == Variant::INT) {
		r_text = String::num_int64((int64_t)p_payload);
		return true;
	}
	if (p_payload.get_type() == Variant::FLOAT) {
		r_text = String::num((double)p_payload);
		return true;
	}
	if (p_payload.get_type() == Variant::DICTIONARY) {
		const Dictionary payload = p_payload;
		if (!payload.has("text")) {
			return false;
		}
		return _payload_text(payload["text"], r_text);
	}
	return false;
}

bool InputSession::_payload_step(const Variant &p_payload, double &r_step) {
	if (p_payload.get_type() == Variant::DICTIONARY) {
		const Dictionary payload = p_payload;
		if (!payload.has("step")) {
			return false;
		}
		const Variant step = payload["step"];
		if (step.get_type() != Variant::INT && step.get_type() != Variant::FLOAT) {
			return false;
		}
		r_step = (double)step;
		return true;
	}
	if (p_payload.get_type() == Variant::INT || p_payload.get_type() == Variant::FLOAT) {
		r_step = (double)p_payload;
		return true;
	}
	if (_payload_is_empty(p_payload)) {
		r_step = 1.0;
		return true;
	}
	return false;
}

bool InputSession::_is_printable_ascii(const String &p_text) {
	for (int i = 0; i < p_text.length(); i++) {
		const char32_t c = p_text[i];
		if (c < 0x20 || c > 0x7e) {
			return false;
		}
	}
	return true;
}

bool InputSession::_insert_text(const String &p_text) {
	if (p_text.is_empty() || !_is_printable_ascii(p_text)) {
		return false;
	}
	String inserted = p_text;
	if (shift_on) {
		inserted = inserted.to_upper();
		shift_on = false;
	}
	text_buffer = text_buffer.substr(0, text_caret) + inserted + text_buffer.substr(text_caret);
	text_caret += inserted.length();
	return true;
}

bool InputSession::_dispatch_numeric_action(const String &p_action_id, const Variant &p_payload) {
	if (p_action_id == "insert_text") {
		String text;
		if (!_payload_text(p_payload, text) || text.is_empty()) {
			return false;
		}
		for (int i = 0; i < text.length(); i++) {
			const char32_t c = text[i];
			if (!((c >= '0' && c <= '9') || c == '.' || c == '-')) {
				return false;
			}
		}
		bool changed = false;
		for (int i = 0; i < text.length(); i++) {
			const char32_t c = text[i];
			if (c >= '0' && c <= '9') {
				tag_keypad_insert_digit(numeric_buffer, c);
				changed = true;
			} else if (c == '.') {
				tag_keypad_insert_dot(numeric_buffer);
				changed = true;
			} else if (c == '-') {
				tag_keypad_toggle_sign(numeric_buffer);
				changed = true;
			}
		}
		return changed;
	}
	if (p_action_id == "decimal") {
		if (!_payload_is_empty(p_payload)) {
			return false;
		}
		tag_keypad_insert_dot(numeric_buffer);
		return true;
	}
	if (p_action_id == "toggle_sign") {
		if (!_payload_is_empty(p_payload)) {
			return false;
		}
		tag_keypad_toggle_sign(numeric_buffer);
		return true;
	}
	if (p_action_id == "backspace") {
		if (!_payload_is_empty(p_payload)) {
			return false;
		}
		tag_keypad_backspace(numeric_buffer);
		return true;
	}
	if (p_action_id == "delete") {
		if (!_payload_is_empty(p_payload)) {
			return false;
		}
		tag_keypad_delete(numeric_buffer);
		return true;
	}
	if (p_action_id == "clear") {
		if (!_payload_is_empty(p_payload)) {
			return false;
		}
		tag_keypad_clear(numeric_buffer);
		return true;
	}
	if (p_action_id == "move_left") {
		if (!_payload_is_empty(p_payload)) {
			return false;
		}
		tag_keypad_move_left(numeric_buffer);
		return true;
	}
	if (p_action_id == "move_right") {
		if (!_payload_is_empty(p_payload)) {
			return false;
		}
		tag_keypad_move_right(numeric_buffer);
		return true;
	}
	if (p_action_id == "move_home") {
		if (!_payload_is_empty(p_payload)) {
			return false;
		}
		numeric_buffer.caret = 0;
		return true;
	}
	if (p_action_id == "move_end") {
		if (!_payload_is_empty(p_payload)) {
			return false;
		}
		numeric_buffer.caret = numeric_buffer.text.length();
		return true;
	}
	if (p_action_id == "increment") {
		double step = 0.0;
		if (!_payload_step(p_payload, step)) {
			return false;
		}
		return tag_keypad_inc(numeric_buffer, step);
	}
	if (p_action_id == "decrement") {
		double step = 0.0;
		if (!_payload_step(p_payload, step)) {
			return false;
		}
		return tag_keypad_dec(numeric_buffer, step);
	}
	return false;
}

bool InputSession::_dispatch_text_action(const String &p_action_id, const Variant &p_payload) {
	if (p_action_id == "toggle_shift") {
		if (!_payload_is_empty(p_payload)) {
			return false;
		}
		shift_on = !shift_on;
		return true;
	}
	if (p_action_id == "insert_text") {
		String text;
		if (!_payload_text(p_payload, text)) {
			return false;
		}
		return _insert_text(text);
	}
	if (p_action_id == "backspace") {
		if (!_payload_is_empty(p_payload)) {
			return false;
		}
		if (text_caret <= 0 || text_buffer.is_empty()) {
			return true;
		}
		text_buffer = text_buffer.substr(0, text_caret - 1) + text_buffer.substr(text_caret);
		text_caret--;
		return true;
	}
	if (p_action_id == "delete") {
		if (!_payload_is_empty(p_payload)) {
			return false;
		}
		if (text_caret >= text_buffer.length()) {
			return true;
		}
		text_buffer = text_buffer.substr(0, text_caret) + text_buffer.substr(text_caret + 1);
		return true;
	}
	if (p_action_id == "clear") {
		if (!_payload_is_empty(p_payload)) {
			return false;
		}
		text_buffer = String();
		text_caret = 0;
		return true;
	}
	if (p_action_id == "move_left") {
		if (!_payload_is_empty(p_payload)) {
			return false;
		}
		text_caret = MAX(text_caret - 1, 0);
		return true;
	}
	if (p_action_id == "move_right") {
		if (!_payload_is_empty(p_payload)) {
			return false;
		}
		text_caret = MIN(text_caret + 1, text_buffer.length());
		return true;
	}
	if (p_action_id == "move_home") {
		if (!_payload_is_empty(p_payload)) {
			return false;
		}
		text_caret = 0;
		return true;
	}
	if (p_action_id == "move_end") {
		if (!_payload_is_empty(p_payload)) {
			return false;
		}
		text_caret = text_buffer.length();
		return true;
	}
	return false;
}

bool InputSession::dispatch_edit_action(const String &p_action_id, const Variant &p_payload) {
	if (!active || !keypad_action_id_is_input(p_action_id)) {
		return false;
	}
	if (p_action_id == "confirm" || p_action_id == "cancel") {
		return false;
	}

	const bool handled = input_mode == "numeric" ?
			_dispatch_numeric_action(p_action_id, p_payload) :
			_dispatch_text_action(p_action_id, p_payload);
	if (handled) {
		modified = true;
	}
	return handled;
}

Dictionary InputSession::_normalize_callback_result(const Variant &p_result, const String &p_default_error) const {
	if (p_result.get_type() == Variant::DICTIONARY) {
		Dictionary result = p_result;
		if (!result.has("ok")) {
			result["ok"] = false;
		}
		if ((bool)result["ok"] && !result.has("value")) {
			result["value"] = get_buffer_text();
		}
		if (!(bool)result["ok"] && !result.has("error")) {
			result["error"] = p_default_error;
		}
		return result;
	}

	Dictionary result;
	if (p_result.get_type() == Variant::BOOL) {
		result["ok"] = (bool)p_result;
		if ((bool)result["ok"]) {
			result["value"] = get_buffer_text();
		}
	} else {
		result["ok"] = false;
		result["error"] = p_default_error;
	}
	if (!(bool)result["ok"]) {
		result["error"] = p_default_error;
	}
	return result;
}

Dictionary InputSession::validate_buffer() const {
	if (!active) {
		Dictionary result;
		result["ok"] = false;
		result["error"] = "session inactive";
		return result;
	}

	if (input_mode == "numeric") {
		const String value = get_buffer_text().strip_edges();
		if (value.is_empty() || !value.is_valid_float()) {
			Dictionary result;
			result["ok"] = false;
			result["value"] = value;
			result["error"] = "invalid number";
			return result;
		}
		const double numeric = value.to_float();
		if (has_min && numeric < min_value) {
			Dictionary result;
			result["ok"] = false;
			result["value"] = value;
			result["error"] = _format_out_of_range_message();
			return result;
		}
		if (has_max && numeric > max_value) {
			Dictionary result;
			result["ok"] = false;
			result["value"] = value;
			result["error"] = _format_out_of_range_message();
			return result;
		}
	}

	if (!validation_callback.is_valid()) {
		Dictionary result;
		result["ok"] = true;
		result["value"] = get_buffer_text();
		return result;
	}

	Array args;
	args.append(get_buffer_text());
	const Variant callback_result = validation_callback.callv(args);
	return _normalize_callback_result(callback_result, "validation failed");
}

Dictionary InputSession::commit_buffer() {
	const Dictionary validation = validate_buffer();
	if (!(bool)validation.get("ok", false)) {
		return validation;
	}
	if (!active) {
		return validation;
	}
	if (commit_callback.is_valid()) {
		Array args;
		args.append(get_buffer_text());
		const Dictionary result = _normalize_callback_result(commit_callback.callv(args), "commit failed");
		if (!(bool)result.get("ok", false)) {
			return result;
		}
	}
	active = false;
	return validation;
}

void InputSession::cancel() {
	if (!active) {
		return;
	}
	active = false;
}
