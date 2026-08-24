#include "widget_trend.h"

#include "core/object/class_db.h"
#include "core/variant/array.h"

WidgetTrend::WidgetTrend() {
	// Default capacity 4096. Pair is trivially default-constructible, so
	// resize() zero-initializes all slots. Safety is guaranteed by the
	// sample_count/head invariant below — stale slots are never read.
	buffer.resize(capacity);
}

// Ring-buffer invariant (fixed capacity, ascending pushes):
//   * Not full (sample_count < capacity): no wrap has ever occurred; the valid
//     samples live contiguously at [0, sample_count), oldest at index 0, and
//     `head == sample_count` (next free slot).
//   * Full (sample_count == capacity): every slot is valid; the oldest sample
//     sits at physical index `head`, logical order is head, head+1, ...,
//     head-1 (mod capacity).
// Both cases: logical index k -> physical (start + k) % capacity, with
// start = (sample_count == capacity) ? head : 0.

void WidgetTrend::push_sample(int64_t ts_ms, double value) {
	ERR_FAIL_COND_MSG(capacity <= 0, "WidgetTrend capacity must be > 0.");
	buffer.set(head, Pair<int64_t, double>(ts_ms, value));
	head = (head + 1) % capacity;
	if (sample_count < capacity) {
		sample_count++;
	}
}

Array WidgetTrend::query_window(int64_t from_ms, int64_t to_ms) const {
	Array out;
	if (from_ms > to_ms) {
		return out;
	}
	const int n = sample_count;
	if (n == 0) {
		return out;
	}
	const int start = (n == capacity) ? head : 0;
	for (int k = 0; k < n; k++) {
		const Pair<int64_t, double> &p = buffer[(start + k) % capacity];
		if (p.first >= from_ms && p.first <= to_ms) {
			Array point;
			point.push_back(p.first);
			point.push_back(p.second);
			out.push_back(point);
		}
	}
	return out;
}

void WidgetTrend::resize(int p_capacity) {
	ERR_FAIL_COND_MSG(p_capacity <= 0, "WidgetTrend capacity must be > 0.");
	if (p_capacity == capacity) {
		return;
	}
	const int keep = MIN(p_capacity, sample_count);
	const int start = (sample_count == capacity) ? head : 0;
	const int src_begin = sample_count - keep; // first logical index to keep

	Vector<Pair<int64_t, double>> preserved;
	preserved.resize(keep);
	for (int j = 0; j < keep; j++) {
		preserved.set(j, buffer[(start + (src_begin + j)) % capacity]);
	}
	buffer = preserved;
	buffer.resize(p_capacity);

	capacity = p_capacity;
	sample_count = keep;
	head = keep % capacity; // next free slot; == 0 when full
}

int WidgetTrend::get_sample_count() const {
	return sample_count;
}

double WidgetTrend::get_min_value() const {
	if (sample_count == 0) {
		return 0.0;
	}
	const int start = (sample_count == capacity) ? head : 0;
	double mn = buffer[start].second;
	for (int k = 1; k < sample_count; k++) {
		const double v = buffer[(start + k) % capacity].second;
		if (v < mn) {
			mn = v;
		}
	}
	return mn;
}

double WidgetTrend::get_max_value() const {
	if (sample_count == 0) {
		return 0.0;
	}
	const int start = (sample_count == capacity) ? head : 0;
	double mx = buffer[start].second;
	for (int k = 1; k < sample_count; k++) {
		const double v = buffer[(start + k) % capacity].second;
		if (v > mx) {
			mx = v;
		}
	}
	return mx;
}

void WidgetTrend::clear() {
	head = 0;
	sample_count = 0;
}

String WidgetTrend::get_version() const {
	return "0.1.1";
}

void WidgetTrend::_bind_methods() {
	ClassDB::bind_method(D_METHOD("push_sample", "ts_ms", "value"), &WidgetTrend::push_sample);
	ClassDB::bind_method(D_METHOD("query_window", "from_ms", "to_ms"), &WidgetTrend::query_window);
	ClassDB::bind_method(D_METHOD("resize", "capacity"), &WidgetTrend::resize);
	ClassDB::bind_method(D_METHOD("get_sample_count"), &WidgetTrend::get_sample_count);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sample_count"), "", "get_sample_count");
	ClassDB::bind_method(D_METHOD("get_min_value"), &WidgetTrend::get_min_value);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_value"), "", "get_min_value");
	ClassDB::bind_method(D_METHOD("get_max_value"), &WidgetTrend::get_max_value);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_value"), "", "get_max_value");
	ClassDB::bind_method(D_METHOD("clear"), &WidgetTrend::clear);
	ClassDB::bind_method(D_METHOD("get_version"), &WidgetTrend::get_version);
}