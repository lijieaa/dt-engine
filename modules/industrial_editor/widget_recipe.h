#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

/// EBPro-aligned recipe core: entry name -> ordered write sequence.
/// All containers follow the module standard (Ruling W6): Vector/String, no STL.
class WidgetRecipe : public RefCounted {
	GDCLASS(WidgetRecipe, RefCounted);

protected:
	static void _bind_methods();

public:
	String get_version() const;

	/// Configure from a Dictionary: { entries: [ { name: String, writes: [
	///   { tag: String, value: Variant }, ... ] }, ... ] }.
	/// Returns false (and clears state) on any structural error.
	bool configure(const Dictionary &p_cfg);

	/// Array of entry names (String), in configured order.
	Array list_entries() const;

	/// Ordered write sequence for the named entry: Array of
	/// { tag: String, value: Variant, order: int }. Empty if unknown entry.
	Array build_write_sequence(const String &p_entry_name) const;

	void clear();

private:
	struct WriteStep {
		String tag;
		Variant value;
	};

	struct Entry {
		String name;
		Vector<WriteStep> writes;
	};

	Vector<Entry> entries;
};