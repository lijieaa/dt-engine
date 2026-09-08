#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class TagCache : public RefCounted {
	GDCLASS(TagCache, RefCounted);

protected:
	static void _bind_methods();

public:
	struct Entry {
		String tag;
		Variant value;
		String quality;
		int64_t ts_ms = 0;
		uint64_t version = 0;
	};

	TagCache() = default;
	~TagCache() override = default;

	// GDScript-visible API ------------------------------------------------

	// Replace (or insert) a batch of values. Each entry is a flat Dictionary
	// from ValueConvert::from_wire_dict(). Emits tag_changed for every tag
	// whose version strictly increased (or is brand new).
	void set_batch(const Array &flats);

	// Single upsert, convenience for GDScript and write_result flow.
	void set_one(const Dictionary &flat);

	// Returns a flat Dictionary copy for `tag`, or empty if not cached.
	// Renamed from `get` to avoid clashing with RefCounted::get on ClassDB
	// registration.
	Dictionary get_entry(const String &tag) const;

	// True when the cache currently holds an entry for `tag`.
	bool has(const String &tag) const;

	// Array<String> of known tags.
	Array keys() const;

	// Total entries currently stored.
	int size() const;

	// Remove all entries and emit cleared().
	void clear();

private:
	mutable HashMap<String, Entry> by_tag;
};
