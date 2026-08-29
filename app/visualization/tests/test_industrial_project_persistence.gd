extends SceneTree
## IndustrialProject persist round-trip: no scan_group keys; hard-reject if present.

const TMP_PATH := "res://tests/industrial_project_persistence_test.tmp.json"

var _failures: Array[String] = []


func _initialize() -> void:
	print("=== test_industrial_project_persistence ===")
	_cleanup_tmp()
	_run()
	_cleanup_tmp()
	if _failures.is_empty():
		print("PASS")
		quit(0)
	else:
		for f in _failures:
			push_error(f)
		quit(1)


func _cleanup_tmp() -> void:
	if FileAccess.file_exists(TMP_PATH):
		var err: int = DirAccess.remove_absolute(ProjectSettings.globalize_path(TMP_PATH))
		if err != OK:
			push_warning("Failed to remove temporary project persistence file: %d" % err)


func _clean_payload() -> Dictionary:
	return {
		"devices": [
			{
				"name": "PLC_1",
				"description": "main plc",
				"driver": 0,
				"enabled": true,
				"ip": "192.168.0.10",
				"port": 102,
				"poll_interval": 200,
				"tags": [
					{
						"name": "Temperature",
						"address": "DB1.DBD0",
						"data_type": 6,
						"writable": false,
						"unit": "degC"
					}
				]
			}
		]
	}


func _assert_no_forbidden_keys(d: Dictionary, where: String) -> void:
	if d.has("scan_groups") or d.has("scan_group"):
		_failures.append("%s emitted forbidden scan_group/scan_groups key" % where)


func _run() -> void:
	if not ClassDB.class_exists("IndustrialProject"):
		_failures.append("IndustrialProject class is not registered")
		return

	var project: RefCounted = ClassDB.instantiate("IndustrialProject")
	if project == null:
		_failures.append("IndustrialProject instantiation returned null")
		return

	var loaded_ok: bool = project.from_dict(_clean_payload())
	if not loaded_ok:
		_failures.append("from_dict rejected a clean payload")
		return

	var out: Dictionary = project.to_dict()
	_assert_no_forbidden_keys(out, "root to_dict")
	var devices: Array = out.get("devices", [])
	if devices.size() != 1:
		_failures.append("expected 1 device, got %d" % devices.size())
		return
	var dev: Dictionary = devices[0]
	_assert_no_forbidden_keys(dev, "device to_dict")
	if dev.get("name", "") != "PLC_1":
		_failures.append("device name was not preserved")
	if int(dev.get("poll_interval", 0)) != 200:
		_failures.append("device poll_interval was not preserved")
	var tags: Array = dev.get("tags", [])
	if tags.size() != 1:
		_failures.append("expected 1 tag, got %d" % tags.size())
		return
	var tag: Dictionary = tags[0]
	_assert_no_forbidden_keys(tag, "tag to_dict")
	if tag.get("name", "") != "Temperature":
		_failures.append("tag name was not preserved")
	if tag.get("address", "") != "DB1.DBD0":
		_failures.append("tag address was not preserved")

	var save_err: int = project.save_to_file(TMP_PATH)
	if save_err != OK:
		_failures.append("save_to_file failed: %d" % save_err)
		return
	var reloaded: RefCounted = ClassDB.instantiate("IndustrialProject")
	var load_err: int = reloaded.load_from_file(TMP_PATH)
	if load_err != OK:
		_failures.append("load_from_file failed: %d" % load_err)
		return
	var round: Dictionary = reloaded.to_dict()
	_assert_no_forbidden_keys(round, "round-trip root")
	if reloaded.get_device_count() != 1:
		_failures.append("round-trip expected 1 device, got %d" % reloaded.get_device_count())

	# Hard-reject: root / device / nested tag. Must not mutate a good project.
	var reject_cases: Array = [
		{"scan_groups": [], "devices": []},
		{"scan_group": "fast", "devices": []},
		{"devices": [{"name": "D", "scan_group": "fast", "tags": []}]},
		{"devices": [{"name": "D", "tags": [{"name": "T", "address": "M10", "scan_group": "fast"}]}]},
	]
	for i in reject_cases.size():
		var ok: bool = project.from_dict(reject_cases[i])
		if ok:
			_failures.append("from_dict must hard-fail forbidden keys (case %d)" % i)
		if project.get_device_count() != 1:
			_failures.append("from_dict must not mutate project on forbidden keys (case %d)" % i)
			return
