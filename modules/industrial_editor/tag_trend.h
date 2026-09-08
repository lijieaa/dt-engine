#pragma once

#include "scene/gui/control.h"

#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"

/// TagTrend â€?engine-native trend/chart widget.
/// Subscribes a tag through Runtime; every tag_changed pushes a sample
/// into the C++ WidgetTrend core (ring buffer), then _draw() renders the
/// window via query_window. C++ port of tag_trend.gd.
class TagTrend : public Control {
	GDCLASS(TagTrend, Control);

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void _draw();

public:
	TagTrend() = default;

	void set_tag_name(const String &p_tag);
	String get_tag_name() const { return tag_name; }

	void set_buffer_capacity(int p_cap);
	int get_buffer_capacity() const { return buffer_capacity; }
	void set_line_color(const Color &c) { line_color = c; queue_redraw(); }
	Color get_line_color() const { return line_color; }
	void set_grid_color(const Color &c) { grid_color = c; queue_redraw(); }
	Color get_grid_color() const { return grid_color; }
	void set_window_ms(int p_ms) { window_ms = MAX(1, p_ms); queue_redraw(); }
	int get_window_ms() const { return window_ms; }

	int get_point_count() const { return point_count; }

	void apply_external_value(const Variant &p_value, int p_ts_ms);
	void _on_tag_changed(const String &tag, const Variant &v, const String &quality, int version, int ts_ms);

private:
	String tag_name;
	int buffer_capacity = 4096;
	Color line_color = Color(0.25f, 0.65f, 0.98f);
	Color grid_color = Color(0.16f, 0.18f, 0.22f);
	int window_ms = 60000;

	int64_t last_ts_ms = 0;
	int64_t detect_ts_ms = 0;
	int point_count = 0;
	Vector<Vector2> points; // normalized (t, v) pairs for drawing

	// Ring-buffer samples: array of (ts_ms, value).
	// We keep a local copy to avoid heavy C++ WidgetTrend marshalling each frame.
	struct Sample {
		int64_t ts_ms = 0;
		float value = 0.0f;
	};
	Vector<Sample> samples;
	int sample_head = 0;
	int sample_count = 0;

	void push_sample(int64_t ts_ms, float value);
	void cache_render_points();
};