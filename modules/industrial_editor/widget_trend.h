#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/pair.h"
#include "core/templates/vector.h"

class WidgetTrend : public RefCounted {
	GDCLASS(WidgetTrend, RefCounted);

protected:
	static void _bind_methods();

private:
	// Ring buffer of (timestamp_ms, value) pairs. `head` points at the next
	// write slot; `sample_count` tracks how many slots currently hold valid
	// samples (<= capacity), so empty slots are never read.
	Vector<Pair<int64_t, double>> buffer;
	int head = 0;
	int capacity = 4096;
	int sample_count = 0;

public:
	WidgetTrend();

	void push_sample(int64_t ts_ms, double value);
	Array query_window(int64_t from_ms, int64_t to_ms) const;
	void resize(int p_capacity);
	int get_sample_count() const;
	double get_min_value() const;
	double get_max_value() const;
	void clear();

	String get_version() const;
};