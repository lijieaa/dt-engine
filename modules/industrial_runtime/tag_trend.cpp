#include "tag_trend.h"

#include "tag_widget_util.h"
#include "widget_trend.h"

#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/variant/callable.h"
#include "scene/main/node.h"

void TagTrend::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tag_name", "tag"), &TagTrend::set_tag_name);
	ClassDB::bind_method(D_METHOD("get_tag_name"), &TagTrend::get_tag_name);
	ClassDB::bind_method(D_METHOD("set_buffer_capacity", "cap"), &TagTrend::set_buffer_capacity);
	ClassDB::bind_method(D_METHOD("get_buffer_capacity"), &TagTrend::get_buffer_capacity);
	ClassDB::bind_method(D_METHOD("set_line_color", "c"), &TagTrend::set_line_color);
	ClassDB::bind_method(D_METHOD("get_line_color"), &TagTrend::get_line_color);
	ClassDB::bind_method(D_METHOD("set_grid_color", "c"), &TagTrend::set_grid_color);
	ClassDB::bind_method(D_METHOD("get_grid_color"), &TagTrend::get_grid_color);
	ClassDB::bind_method(D_METHOD("set_window_ms", "ms"), &TagTrend::set_window_ms);
	ClassDB::bind_method(D_METHOD("get_window_ms"), &TagTrend::get_window_ms);
	ClassDB::bind_method(D_METHOD("get_point_count"), &TagTrend::get_point_count);
	ClassDB::bind_method(D_METHOD("apply_external_value", "value", "ts_ms"), &TagTrend::apply_external_value);
	ClassDB::bind_method(D_METHOD("_on_tag_changed", "tag", "v", "quality", "version", "ts_ms"), &TagTrend::_on_tag_changed);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tag_name"), "set_tag_name", "get_tag_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "buffer_capacity"), "set_buffer_capacity", "get_buffer_capacity");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "line_color"), "set_line_color", "get_line_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "grid_color"), "set_grid_color", "get_grid_color");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "window_ms"), "set_window_ms", "get_window_ms");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "point_count"), "", "get_point_count");
}

void TagTrend::set_tag_name(const String &p_tag) {
	tag_name = p_tag;
	if (tag_name.is_empty()) {
		return;
	}
	tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagTrend::_on_tag_changed));
}

void TagTrend::set_buffer_capacity(int p_cap) {
	buffer_capacity = MAX(32, p_cap);
	samples.resize(buffer_capacity);
	point_count = 0;
	queue_redraw();
}

void TagTrend::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		set_custom_minimum_size(Size2(220, 120));
		samples.resize(buffer_capacity);
		if (!tag_name.is_empty()) {
			tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagTrend::_on_tag_changed));
		}
		queue_redraw();
	} else if (p_what == NOTIFICATION_DRAW) {
		cache_render_points();
		// Background.
		const Size2 sz = get_size();
		draw_rect(Rect2(Vector2(), sz), Color(0.05f, 0.06f, 0.08f));
		// Grid: 6 horizontal lines.
		for (int i = 0; i < 6; i++) {
			const float y = sz.y * float(i) / 5.0f;
			draw_line(Vector2(0, y), Vector2(sz.x, y), grid_color, 1.0f);
		}
		if (points.size() < 2) {
			return;
		}
		// Value range estimation.
		float lo = INFINITY;
		float hi = -INFINITY;
		for (const Vector2 &p : points) {
			lo = MIN(lo, p.y);
			hi = MAX(hi, p.y);
		}
		if (hi - lo < 0.001f) {
			hi = lo + 1.0f;
		}
		const float ts0 = points[0].x;
		const float ts1 = points[points.size() - 1].x;
		const float span = (ts1 > ts0) ? (ts1 - ts0) : 1.0f;
		// Polyline pass.
		Vector<Point2> line;
		line.resize(points.size());
		for (int i = 0; i < points.size(); i++) {
			const float t = (points[i].x - ts0) / span;
			const float x = t * sz.x;
			const float y = sz.y - (points[i].y - lo) / (hi - lo) * (sz.y - 4.0f) - 2.0f;
			line.set(i, Vector2(x, y));
		}
		draw_polyline(line, line_color, 1.5f, false);
	}
}

void TagTrend::_on_tag_changed(const String &tag, const Variant &v, const String &quality, int version, int ts_ms) {
	if (tag != tag_name) {
		return;
	}
	apply_external_value(v, ts_ms);
}

void TagTrend::apply_external_value(const Variant &p_value, int p_ts_ms) {
	const float f = tag_widget::to_float(p_value, 0.0f);
	OS *os = OS::get_singleton();
	const int64_t ts = (p_ts_ms > 0) ? int64_t(p_ts_ms) : (os ? (int64_t)(os->get_ticks_msec()) : 0);
	push_sample(ts, f);
	queue_redraw();
}

void TagTrend::push_sample(int64_t ts_ms, float value) {
	if (samples.size() == 0) {
		samples.resize(buffer_capacity);
	}
	samples.set(sample_head, Sample{ ts_ms, value });
	sample_head = (sample_head + 1) % buffer_capacity;
	if (sample_count < buffer_capacity) {
		sample_count++;
	}
	last_ts_ms = ts_ms;
}

void TagTrend::cache_render_points() {
	points.clear();
	if (sample_count == 0) {
		point_count = 0;
		return;
	}
	// The window is [now - window_ms, now]. Use the drawing time base.
	OS *os = OS::get_singleton();
	const int64_t now = os ? (int64_t)(os->get_ticks_msec()) : 0;
	const int64_t from = now - int64_t(window_ms);
	points.reserve(sample_count);
	// Iterate in logical order (oldest -> newest).
	const int start = (sample_count == buffer_capacity) ? sample_head : 0;
	for (int k = 0; k < sample_count; k++) {
		const Sample &s = samples[(start + k) % buffer_capacity];
		if (s.ts_ms >= from && s.ts_ms <= now) {
			points.push_back(Vector2(float(s.ts_ms), s.value));
		}
	}
	point_count = points.size();
}