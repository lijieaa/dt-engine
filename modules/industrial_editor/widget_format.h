#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"

#include "core/variant/dictionary.h"

class WidgetFormat : public RefCounted {
	GDCLASS(WidgetFormat, RefCounted);

protected:
	static void _bind_methods();

public:
	/// Format a raw value according to a numeric format config.
	/// cfg keys: decimals:int, thousands:bool, prefix:String, suffix:String,
	///           bcd:bool, signed:bool
	static String format_value(const Variant &raw, const Dictionary &cfg);

	/// Parse user text input into a numeric Variant.
	/// Returns { ok: bool, value: Variant, error: String }.
	static Dictionary parse_input(const String &text, const Dictionary &cfg);

	String get_version() const;

private:
	/// Decode an integer as packed BCD (each byte = high nibble tens + low nibble ones).
	/// p_is_signed: top F-nibble is a sign marker (0xF -> negative, EBPro style).
	static String decode_bcd(uint64_t p_raw, bool p_is_signed);
	/// Insert a comma thousands separator into a numeric string (respects leading '-').
	static String add_thousands(const String &p_num);
	/// Parse text into an integer using the given base, validating every digit.
	static bool to_integer(const String &p_text, int p_base, int64_t &r_value);
};