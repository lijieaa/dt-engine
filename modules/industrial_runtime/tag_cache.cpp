#include "tag_cache.h"

#include "core/object/class_db.h"
#include "core/object/method_info.h"
#include "core/variant/array.h"

void TagCache::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_batch", "flat_values"), &TagCache::set_batch);
	ClassDB::bind_method(D_METHOD("set_one", "flat_value"), &TagCache::set_one);
	ClassDB::bind_method(D_METHOD("get_entry", "tag"), &TagCache::get_entry);
	ClassDB::bind_method(D_METHOD("has", "tag"), &TagCache::has);
	ClassDB::bind_method(D_METHOD("keys"), &TagCache::keys);
	ClassDB::bind_method(D_METHOD("size"), &TagCache::size);
	ClassDB::bind_method(D_METHOD("clear"), &TagCache::clear);

	ADD_SIGNAL(MethodInfo("tag_changed",
			PropertyInfo(Variant::STRING, "tag"),
			PropertyInfo(Variant::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT),
			PropertyInfo(Variant::STRING, "quality"),
			PropertyInfo(Variant::INT, "version"),
			PropertyInfo(Variant::INT, "ts_ms")));
	ADD_SIGNAL(MethodInfo("cleared"));
}

void TagCache::set_batch(const Array &flats) {
	for (int i = 0; i < flats.size(); i++) {
		Dictionary d = flats[i];
		set_one(d);
	}
}

void TagCache::set_one(const Dictionary &flat) {
	if (!flat.has("tag")) {
		return;
	}
	String tag = flat["tag"];
	if (tag.length() == 0) {
		return;
	}
	Entry entry{};
	entry.tag = tag;
	entry.value = flat.get("value", Variant());
	entry.quality = flat.has("quality") ? String(flat["quality"]) : String("stale");
	entry.ts_ms = flat.has("ts_ms") ? int64_t(flat["ts_ms"]) : 0;
	entry.version = flat.has("version") ? uint64_t(int64_t(flat.get("version", int64_t(0)))) : 0;

	bool emit = false;
	Entry *existing = by_tag.getptr(tag);
	if (existing == nullptr) {
		by_tag.insert(tag, entry);
		emit = true;
	} else {
		// Bump local version if the incoming value didn't carry one but the
		// entry is different. This lets UI bindings detect "value-only"
		// updates that the server sometimes emits without explicit version.
		if (entry.version == 0 && existing->version > 0 && existing->value != entry.value) {
			entry.version = existing->version + 1;
		}
		bool newer = entry.version > existing->version;
		bool value_or_quality_changed = (existing->value != entry.value) || (existing->quality != entry.quality);
		if (newer || (entry.version == existing->version && value_or_quality_changed)) {
			*existing = entry;
			emit = true;
		} else {
			*existing = entry;
		}
	}
	if (emit) {
		Variant args[5] = { tag, entry.value, entry.quality, int64_t(entry.version), entry.ts_ms };
		const Variant *argv[5] = { &args[0], &args[1], &args[2], &args[3], &args[4] };
		emit_signalp("tag_changed", argv, 5);
	}
}

Dictionary TagCache::get_entry(const String &tag) const {
	const Entry *e = by_tag.getptr(tag);
	if (e == nullptr) {
		return Dictionary();
	}
	Dictionary d;
	d["tag"] = e->tag;
	d["value"] = e->value;
	d["quality"] = e->quality;
	d["ts_ms"] = e->ts_ms;
	d["version"] = int64_t(e->version);
	return d;
}

bool TagCache::has(const String &tag) const {
	return by_tag.has(tag);
}

Array TagCache::keys() const {
	Array out;
	for (KeyValue<String, Entry> &kv : by_tag) {
		out.push_back(kv.key);
	}
	return out;
}

int TagCache::size() const {
	return int(by_tag.size());
}

void TagCache::clear() {
	by_tag.clear();
	emit_signalp("cleared", nullptr, 0);
}
