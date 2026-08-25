#pragma once
/// String obfuscation helpers (Task 9 hardening).
///
/// Error messages / keys that are shown to users or embedded in the binary
/// are stored as XOR-encoded byte tables so they do not appear as plaintext
/// in a static dump. Encoding happens offline; at runtime only `decode()`
/// is used. All storage is Godot String (Ruling W6 — no STL).
#include "core/string/ustring.h"
#include "core/templates/vector.h" // Godot Vector<uint8_t>

namespace widget_obfuscate {

/// XOR-decode an encoded byte sequence back into a String (UTF-8).
/// `p_encoded` holds raw bytes as produced by the encode-time step;
/// `p_key` is a short byte array; the result preserves any non-ASCII bytes.
inline String decode(const Vector<uint8_t> &p_encoded, const Vector<uint8_t> &p_key) {
	if (p_encoded.is_empty()) {
		return String();
	}
	Vector<uint8_t> out;
	out.resize(p_encoded.size());
	const int key_size = p_key.size() > 0 ? p_key.size() : 1;
	for (int i = 0; i < p_encoded.size(); i++) {
		out.set(i, p_encoded[i] ^ p_key[i % key_size]);
	}
	// Interpret as UTF-8 (error-tolerant; falls back to Latin-1 on bad bytes).
	return String::utf8((const char *)out.ptr(), out.size());
}

/// Convenience: decode from a pair of C-style byte arrays (avoids needing
/// Vector construction in callers for small fixed strings).
inline String decode(const uint8_t *p_encoded, int p_len, const uint8_t *p_key, int p_key_len) {
	Vector<uint8_t> enc;
	enc.resize(p_len);
	for (int i = 0; i < p_len; i++) {
		enc.set(i, p_encoded[i]);
	}
	Vector<uint8_t> key;
	key.resize(p_key_len);
	for (int i = 0; i < p_key_len; i++) {
		key.set(i, p_key[i]);
	}
	return decode(enc, key);
}

} // namespace widget_obfuscate