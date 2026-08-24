extends SceneTree
## Widget C++ skeleton load test (Task 3).
## Verifies all 6 widget classes are registered in ClassDB and instantiable.

func _initialize() -> void:
	print("=== test_native_load ===")
	var failures := []
	var classes := ["WidgetFormat", "WidgetTrend", "WidgetAlarm", "WidgetMeter", "WidgetRecipe", "WidgetMacro"]
	for c in classes:
		if ClassDB.class_exists(c):
			print("  PASS: ", c, " class exists")
			var inst = ClassDB.instantiate(c)
			if inst != null:
				var v = inst.get_version()
				if v == "0.1.0":
					print("  PASS: ", c, ".get_version() == 0.1.0")
				else:
					print("  FAIL: ", c, ".get_version() returned ", v)
					failures.append(c + " version mismatch")
				# RefCounted — no free() needed
			else:
				print("  FAIL: ", c, " instantiation returned null")
				failures.append(c + " null instance")
		else:
			print("  FAIL: ", c, " class not found in ClassDB")
			failures.append(c + " not found")
	if failures.is_empty():
		print("RESULT: PASS — all %d checks OK" % (classes.size() * 2))
		quit(0)
	else:
		print("RESULT: FAIL — %d failure(s)" % failures.size())
		for f in failures:
			print("  FAIL: ", f)
		quit(1)
