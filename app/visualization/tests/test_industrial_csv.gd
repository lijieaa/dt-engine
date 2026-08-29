extends SceneTree
## CSV export has no ScanGroup column; import hard-rejects that header.

const DEV_CSV := "res://tests/industrial_csv_devices_test.tmp.csv"
const TAG_CSV := "res://tests/industrial_csv_tags_test.tmp.csv"
const REJECT_DEV := "res://tests/industrial_csv_reject_devices.tmp.csv"
const REJECT_TAG := "res://tests/industrial_csv_reject_tags.tmp.csv"

var _failures: Array[String] = []


func _initialize() -> void:
	print("=== test_industrial_csv ===")
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


func _tmp_paths() -> Array[String]:
	return [DEV_CSV, TAG_CSV, REJECT_DEV, REJECT_TAG]


func _cleanup_tmp() -> void:
	for p in _tmp_paths():
		if FileAccess.file_exists(p):
			var err: int = DirAccess.remove_absolute(ProjectSettings.globalize_path(p))
			if err != OK:
				push_warning("Failed to remove temporary CSV file %s: %d" % [p, err])


func _write_text(path: String, text: String) -> void:
	var f: FileAccess = FileAccess.open(path, FileAccess.WRITE)
	if f == null:
		_failures.append("could not write %s" % path)
		return
	f.store_string(text)
	f.close()


func _read_first_line(path: String) -> String:
	var f: FileAccess = FileAccess.open(path, FileAccess.READ)
	if f == null:
		return ""
	var line: String = f.get_line()
	f.close()
	return line


func _run() -> void:
	if not ClassDB.class_exists("IndustrialProject"):
		_failures.append("IndustrialProject class is not registered")
		return

	var project: RefCounted = ClassDB.instantiate("IndustrialProject")
	if project == null:
		_failures.append("IndustrialProject instantiation returned null")
		return

	var loaded_ok: bool = project.from_dict({
		"devices": [
			{
				"name": "PLC_1",
				"description": "main plc",
				"driver": 0,
				"enabled": true,
				"tags": [
					{
						"name": "Temperature",
						"address": "DB1.DBD0",
						"data_type": 6,
						"writable": false,
						"scale": 1.0,
						"unit": "degC"
					}
				]
			}
		]
	})
	if not loaded_ok:
		_failures.append("from_dict rejected a clean payload")
		return

	var exp_dev: int = project.export_devices_csv(DEV_CSV)
	if exp_dev != OK:
		_failures.append("export_devices_csv failed: %d" % exp_dev)
		return
	var dev_header: String = _read_first_line(DEV_CSV)
	if dev_header != "Name,Description,Driver,Enabled,ConnectionParams,TagCount":
		_failures.append("device CSV header mismatch: %s" % dev_header)
	if "ScanGroup" in dev_header:
		_failures.append("device CSV header must not contain ScanGroup")

	var exp_tag: int = project.export_tags_csv(TAG_CSV)
	if exp_tag != OK:
		_failures.append("export_tags_csv failed: %d" % exp_tag)
		return
	var tag_header: String = _read_first_line(TAG_CSV)
	if tag_header != "Device,Address,Name,DataType,Writable,Scale,Unit":
		_failures.append("tag CSV header mismatch: %s" % tag_header)
	if "ScanGroup" in tag_header:
		_failures.append("tag CSV header must not contain ScanGroup")

	_write_text(REJECT_DEV, "Name,Description,Driver,ScanGroup,Enabled,ConnectionParams,TagCount\n")
	var rej_dev: int = project.import_devices_csv(REJECT_DEV)
	if rej_dev != ERR_INVALID_DATA:
		_failures.append("import_devices_csv must hard-fail ScanGroup header, got %d" % rej_dev)
	if project.get_device_count() != 1:
		_failures.append("import_devices_csv must not mutate project on ScanGroup header")

	_write_text(REJECT_TAG, "Device,Address,Name,DataType,ScanGroup,Writable,Scale,Unit\n")
	var rej_tag: int = project.import_tags_csv(REJECT_TAG)
	if rej_tag != ERR_INVALID_DATA:
		_failures.append("import_tags_csv must hard-fail ScanGroup header, got %d" % rej_tag)
	if project.get_device_count() != 1:
		_failures.append("import_tags_csv must not mutate project on ScanGroup header")

	var roundtrip: RefCounted = ClassDB.instantiate("IndustrialProject")
	_write_text(DEV_CSV, "Name,Description,Driver,Enabled,ConnectionParams,TagCount\n\"PLC_CSV\",\"desc\",Siemens S7-1200/S7-1500,true,{},0\n")
	var imp: int = roundtrip.import_devices_csv(DEV_CSV)
	if imp != OK:
		_failures.append("import_devices_csv of clean file failed: %d" % imp)
		return
	if roundtrip.get_device_count() != 1:
		_failures.append("expected 1 imported device, got %d" % roundtrip.get_device_count())

	_write_text(TAG_CSV, "Device,Address,Name,DataType,Writable,Scale,Unit\n\"PLC_CSV\",\"M10\",\"TagA\",Int16,true,1,\"C\"\n")
	var imp_tag: int = roundtrip.import_tags_csv(TAG_CSV)
	if imp_tag != OK:
		_failures.append("import_tags_csv of clean file failed: %d" % imp_tag)
