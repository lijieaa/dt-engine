#include "widget_format.h"

#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/variant/variant.h"

void WidgetFormat::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_version"), &WidgetFormat::get_version);

	ClassDB::bind_static_method("WidgetFormat", D_METHOD("format_value", "raw", "cfg"), &WidgetFormat::format_value);
	ClassDB::bind_static_method("WidgetFormat", D_METHOD("parse_input", "text", "cfg"), &WidgetFormat::parse_input);
}

String WidgetFormat::get_version() const {
	return "0.1.0";
}

String WidgetFormat::format_value(const Variant &raw, const Dictionary &cfg) {
	// --- Read format config with defaults (never throw on bad types) ---
	int decimals = 0;
	bool thousands = false;
	bool bcd = false;
	bool is_signed = false;
	String prefix;
	String suffix;

	if (cfg.has("decimals")) {
		decimals = (int)cfg["decimals"];
	}
	if (cfg.has("thousands")) {
		thousands = (bool)cfg["thousands"];
	}
	if (cfg.has("bcd")) {
		bcd = (bool)cfg["bcd"];
	}
	if (cfg.has("signed")) {
		is_signed = (bool)cfg["signed"];
	}
	if (cfg.has("prefix")) {
		prefix = (String)cfg["prefix"];
	}
	if (cfg.has("suffix")) {
		suffix = (String)cfg["suffix"];
	}
	if (decimals < 0) {
		decimals = 0;
	}
	if (decimals > 64) {
		decimals = 64;
	}

	String result;

	if (bcd) {
		// BCD decode: treat the raw integer's bytes as packed decimal digits.
		// Each byte = high nibble (tens) + low nibble (ones); e.g. 0x12 -> "12".
		uint64_t val = 0;
		switch (raw.get_type()) {
			case Variant::INT:
				val = (uint64_t)(int64_t)raw;
				break;
			case Variant::FLOAT:
				val = (uint64_t)(int64_t)(double)raw;
				break;
			default:
				val = (uint64_t)(int64_t)raw.operator int64_t();
				break;
		}
		result = decode_bcd(val, is_signed);
	} else {
		// Standard numeric formatting.
		double num = 0.0;
		switch (raw.get_type()) {
			case Variant::INT:
				num = (double)(int64_t)raw;
				// Preserve integer exactly when no decimals requested.
				if (decimals == 0) {
					result = String::num_int64((int64_t)raw);
				} else {
					result = String::num(num, decimals);
				}
				break;
			case Variant::FLOAT:
				num = (double)raw;
				result = String::num(num, decimals);
				break;
			case Variant::BOOL:
				result = String::num_int64((int64_t)raw);
				break;
			default:
				// Any other type gets coerced to int64 (0 for non-numeric strings).
				if (raw.get_type() == Variant::STRING) {
					String s = (String)raw;
					bool is_num = s.is_valid_int() || s.is_valid_float();
					if (!is_num) {
						result = "0";
						break;
					}
				}
				if (decimals == 0) {
					result = String::num_int64((int64_t)raw);
				} else {
					result = String::num((double)raw, decimals);
				}
				break;
		}
	}

	if (thousands) {
		result = add_thousands(result);
	}

	if (!prefix.is_empty()) {
		result = prefix + result;
	}
	if (!suffix.is_empty()) {
		result = result + suffix;
	}

	return result;
}

Dictionary WidgetFormat::parse_input(const String &text, const Dictionary &cfg) {
	Dictionary result;
	result["ok"] = false;
	result["value"] = Variant();
	result["error"] = String();

	// Empty string is never valid.
	String trimmed = text.strip_edges();
	if (trimmed.is_empty()) {
		result["error"] = TTRC("输入为空");
		return result;
	}

	// Reject any string containing characters that aren't part of a number.
	// Iterate by code points, but we only care about ASCII here.
	static const String ALLOWED = "0123456789+-.eE";
	for (int i = 0; i < trimmed.length(); i++) {
		char32_t c = trimmed[i];
		if (ALLOWED.find(String::chr(c)) == -1) {
			result["error"] = TTRC("包含非法字符");
			return result;
		}
	}

	// --- Determine expected type from cfg: int if decimals == 0, else float ---
	int decimals = 0;
	if (cfg.has("decimals")) {
		decimals = (int)cfg["decimals"];
	}
	if (decimals < 0) {
		decimals = 0;
	}

	if (decimals == 0) {
		// Integer parse. Accept 0x/0b/0o prefixes as well as plain decimal.
		String s = trimmed.to_lower();
		String digits;
		int base = 10;
		if (s.begins_with("0x")) {
			base = 16;
			digits = s.substr(2);
		} else if (s.begins_with("0b")) {
			base = 2;
			digits = s.substr(2);
		} else if (s.begins_with("0o")) {
			base = 8;
			digits = s.substr(2);
		} else {
			digits = s;
		}

		bool neg = false;
		if (digits.begins_with("-")) {
			neg = true;
			digits = digits.substr(1);
		} else if (digits.begins_with("+")) {
			digits = digits.substr(1);
		}
		if (digits.is_empty()) {
			result["error"] = TTRC("没有有效数字");
			return result;
		}

		// Also reject float-ish input when decimals == 0 (e.g. "3.5").
		if (digits.contains(".") || digits.contains("e") || digits.contains("E")) {
			result["error"] = TTRC("需要整数，收到浮点数");
			return result;
		}

		int64_t val = 0;
		if (!to_integer(digits, base, val)) {
			result["error"] = TTRC("数字越界或非法");
			return result;
		}
		if (neg) {
			val = -val;
		}
		result["ok"] = true;
		result["value"] = val;
	} else {
		// Float parse.
		bool valid = trimmed.is_valid_float();
		if (!valid) {
			result["error"] = TTRC("不是有效的浮点数");
			return result;
		}
		double d = (double)trimmed.to_float();
		// Round to the configured number of decimals.
		double factor = 1.0;
		for (int i = 0; i < decimals; i++) {
			factor *= 10.0;
		}
		d = Math::round(d * factor) / factor;
		result["ok"] = true;
		result["value"] = d;
	}

	return result;
}

String WidgetFormat::decode_bcd(uint64_t p_raw, bool p_is_signed) {
	if (p_raw == 0) {
		return "0";
	}
	bool negative = false;
	// Process from the most-significant byte that has data down to byte 0.
	// each byte = high nibble (tens) + low nibble (ones), e.g. 0x12 -> "12",
	// 0x1234 -> "1234".  Leading zero digits of the top byte are suppressed.
	int num_bytes = 0;
	uint64_t t = p_raw;
	while (t != 0) {
		num_bytes++;
		t >>= 8;
	}
	String ret;
	for (int b = num_bytes - 1; b >= 0; b--) {
		uint8_t byte = (uint8_t)((p_raw >> (b * 8)) & 0xFF);
		uint8_t tens = (byte >> 4) & 0x0F;
		uint8_t ones = byte & 0x0F;
		if (p_is_signed && b == num_bytes - 1) {
			// Signed BCD convention (EBPro F-nibble style): the top nibble is a
			// sign marker — 0xF means negative, otherwise it's a magnitude digit.
			if (tens == 0xF) {
				negative = true;
			} else if (tens != 0) {
				ret += String::chr('0' + tens);
			}
		} else if (ret.is_empty()) {
			// Most-significant byte (unsigned path): drop a leading zero tens digit.
			if (tens != 0) {
				ret += String::chr('0' + tens);
			}
		} else {
			ret += String::chr('0' + tens);
		}
		ret += String::chr('0' + ones);
	}
	return (negative ? "-" : "") + ret;
}

String WidgetFormat::add_thousands(const String &p_num) {
	if (p_num.is_empty()) {
		return p_num;
	}
	String sign;
	String body = p_num;
	if (body.begins_with("-")) {
		sign = "-";
		body = body.substr(1);
	}
	// Split integer part from any fractional part.
	String int_part = body;
	String frac_part;
	int dot = body.find(".");
	if (dot != -1) {
		int_part = body.substr(0, dot);
		frac_part = body.substr(dot);
	}
	String out;
	int len = int_part.length();
	for (int i = 0; i < len; i++) {
		if (i > 0 && (len - i) % 3 == 0) {
			out += ",";
		}
		out += int_part[i];
	}
	return sign + out + frac_part;
}

bool WidgetFormat::to_integer(const String &p_text, int p_base, int64_t &r_value) {
	if (p_text.is_empty()) {
		return false;
	}
	int64_t acc = 0;
	for (int i = 0; i < p_text.length(); i++) {
		char32_t c = p_text[i];
		int digit = -1;
		if (c >= '0' && c <= '9') {
			digit = c - '0';
		} else if (c >= 'a' && c <= 'f') {
			digit = 10 + (c - 'a');
		} else if (c >= 'A' && c <= 'F') {
			digit = 10 + (c - 'A');
		}
		if (digit < 0 || digit >= p_base) {
			return false;
		}
		if (acc > (INT64_MAX - digit) / p_base) {
			return false;
		}
		acc = acc * p_base + digit;
	}
	r_value = acc;
	return true;
}