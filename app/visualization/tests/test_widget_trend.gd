extends SceneTree
## WidgetTrend C++ ring-buffer history core test (Task 5).
## Runs headless: godot --headless --script res://tests/test_widget_trend.gd

func _initialize() -> void:
	print("=== test_widget_trend ===")
	var failures := []
	var checks := 0

	# Instantiate via ClassDB (class is compiled into the engine binary).
	if not ClassDB.class_exists("WidgetTrend"):
		printerr("FATAL: WidgetTrend class not registered in ClassDB")
		quit(1)
		return
	var wt: RefCounted = ClassDB.instantiate("WidgetTrend")
	if wt == null:
		printerr("FATAL: WidgetTrend instantiation returned null")
		quit(1)
		return

	# --- Basic: empty trend ---
	checks += 1
	expect_eq(failures, "default sample_count", wt.sample_count, 0)

	# --- Push 5 samples, query window ---
	wt.resize(10)
	for i in range(5):
		wt.push_sample(i * 100, float(i))
	checks += 1
	expect_eq(failures, "sample_count after 5 pushes", wt.sample_count, 5)

	checks += 1
	var w: Array = wt.query_window(100, 400)
	if w.size() != 4:
		failures.append("query_window(100,400).size() expected 4, got %s" % w.size())
	else:
		var ok := true
		for i in range(4):
			var pair: Array = w[i]
			var exp_ts: int = (i + 1) * 100
			var exp_val: float = float(i + 1)
			if pair[0] != exp_ts or absf(pair[1] - exp_val) > 0.0001:
				ok = false
				failures.append(
					"query_window(100,400)[%d] expected [%d, %s], got [%s, %s]"
					% [i, exp_ts, str(exp_val), str(pair[0]), str(pair[1])])
		if ok:
			print("  PASS: query_window(100, 400) -> ", w.size(), " samples ascending")

	checks += 1
	expect_eq(failures, "min_value", wt.min_value, 0.0)
	checks += 1
	expect_eq(failures, "max_value", wt.max_value, 4.0)

	# --- Window boundaries: inclusive ---
	checks += 1
	var w2: Array = wt.query_window(0, 0)
	expect_eq(failures, "query_window(0,0) inclusive single", w2.size(), 1)

	# --- No match window ---
	checks += 1
	expect_eq(failures, "query_window(2000,3000) empty", wt.query_window(2000, 3000).size(), 0)

	# --- Round-trip persistence via Array output ---
	wt.clear()
	checks += 1
	expect_eq(failures, "sample_count after clear", wt.sample_count, 0)
	checks += 1
	expect_eq(failures, "min_value after clear", wt.min_value, 0.0)
	checks += 1
	expect_eq(failures, "max_value after clear", wt.max_value, 0.0)

	# --- Ring overwrite: push more than capacity, oldest dropped ---
	wt.resize(5)
	for i in range(7):
		wt.push_sample(i * 10, float(i))
	checks += 1
	expect_eq(failures, "sample_count after 7 pushes cap 5", wt.sample_count, 5)
	checks += 1
	expect_eq(failures, "max_value after ring wrap", wt.max_value, 6.0)
	var oldest: int = wt.query_window(-1, 999999).size()
	checks += 1
	expect_eq(failures, "query_window whole ring size", oldest, 5)

	print("RESULT: %s — %d checks, %d failure(s)"
		% ["PASS" if failures.is_empty() else "FAIL", checks, failures.size()])
	for f in failures:
		print("  FAIL: ", f)
	quit(0 if failures.is_empty() else 1)


## Append to failures when actual != expected (Float-aware compare).
func expect_eq(failures: Array, label: String, actual: Variant, expected: Variant) -> void:
	if typeof(actual) == TYPE_FLOAT or typeof(expected) == TYPE_FLOAT:
		var av: float = actual
		var ev: float = expected
		if absf(av - ev) > 0.0001:
			failures.append("%s expected '%s' (%s), got '%s' (%s)" % [
				label, str(expected), typeof(expected), str(actual), typeof(actual),
			])
			return
		print("  PASS: ", label, " -> ", expected)
		return
	if actual != expected:
		failures.append("%s expected '%s' (%s), got '%s' (%s)" % [
			label, str(expected), typeof(expected), str(actual), typeof(actual),
		])
	else:
		print("  PASS: ", label, " -> ", actual)