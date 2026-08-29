# Per-device poll acceptance log (spec §4)

Date: 2026-08-29  
Plan: `docs/superpowers/plans/2026-08-29-per-device-poll-no-scan-group.md`  
Spec: `docs/superpowers/specs/2026-08-29-per-device-poll-no-scan-group-design.md`

## Environment

| Repo | Branch | HEAD |
|------|--------|------|
| `D:\driver-engine` | `feat/project-import-tag-schema` | `953212c` `feat(api): drop scan_group from tags/import; refresh examples` |
| `D:\dt-engine\.worktrees\project-import-tag-schema` | `feat/project-import-tag-schema` | `6038df0b03` `feat(industrial_editor): drop Scan Group UI and CSV columns` |

Godot binary used for editor tests:  
`D:\dt-engine\.worktrees\project-import-tag-schema\bin\godot.windows.editor.x86_64.exe`  
(mtime 2026-08-29 17:48; banner `Godot Engine v4.8.dev.custom_build.e2ea0a4aa`)

Live S7: skipped via `S7_SKIP_INTEGRATION=1` (127.0.0.1:102 refused).  
Live Modbus 127.0.0.1:502: TCP accepts then RST — not a working slave (same as T6).

No plan-caused bugs found; no product code changed in this task. Two throwaway probes (`TestScheduler_TwoDevicesDifferentPollIntervals`, `TestAccept_DeviceCRUD_CorrectMethods`) were run then deleted so they are not committed.

---

## Checklist

### Step 1 — `go test ./...` — **FAIL** (pre-existing; plan packages green)

```
$env:S7_SKIP_INTEGRATION='1'
go test ./... -count=1
```

Plan-scoped packages **PASS**: `internal/project`, `internal/tags`, `internal/collector`, `internal/scheduler`, `internal/runtime`, `internal/api`, plus `internal/ws`, `internal/license`, `internal/cache`, `internal/logger`, `internal/drivers/s7`, `internal/drivers/mitsubishi`, `cmd/runtime`.

S7 live tests **SKIP** (4): `TestIntegrationLocalReadAreas`, `TestIntegrationLocalWriteThenReadAreas`, `TestIntegrationLocalTypedAddressWriteThenRead`, `TestIntegrationLocalTypedAddressBatchWriteThenRead`.

**FAIL tests (7)** — none caused by dropping `scan_group` (T6 already listed 6 of these; the 7th is modbus merge, untracked by T6 because T6 did not run `./internal/drivers/modbus`):

| Test | Snippet | Pre-existing? |
|------|---------|---------------|
| `TestCreateDevice_CrudLifeCycle` | `crud_test.go:78: PUT enable status=404 want 200 or 503` — test uses `POST /devices/:id`; route is `PUT` | Yes (T6) |
| `TestCreateDevice_MissingFields` | `id+driver` without endpoint → **201** (ip/port synthesis), test wants 400 | Yes (T6) |
| `TestLicenseStatusDemoMode` | `used_devices=3 want 1` — leftover devices from failed CRUD | Yes (T6) |
| `TestGodotClientSmoke` | `Failed loading resource: res://scripts/panels/TagTreePanel.gd` (and StatusBar/MainController) | Yes (T6) |
| `TestModbusLive` | `127.0.0.1:502` RST: `short response` / `connection was forcibly closed` | Yes (T6) |
| `TestModbusAllAddressTypes` | `ir_val: got 100 (float64) want 1000` (fake-server value) | Yes (T6) |
| `TestSlaveIDAffectsFrameBytes` | panic `slice bounds out of range [:1] with capacity 0` in `merge.go:89` (`readMergedHolding`; address `Size=0`) | Yes — `fbab98f` / merge path; **not** this plan |

`TestGodotIndustrialEditorSmoke` **PASS** (9.79s) — plugin `_enter_tree (smoke ok)`.  
`TestModbusSmoke` **PASS**.

Packages with no tests (reported skip): `cmd/demo`, `cmd/genkey`, `cmd/licgen`, `internal/auth`, `internal/device`, `internal/diagnostics`, `internal/driver`, `internal/storage`, `internal/writer`.

---

### Step 2 — Import `scan_groups` → 400; clean import OK; no `scan_groups` in response — **PASS**

```
go test ./internal/api ./tests/integration -count=1 -run "TestProjectImport_"
```

```
--- PASS: TestProjectImport_RejectsScanGroups (0.00s)
--- PASS: TestProjectImport_OK_NoScanGroupFieldInResponse (0.00s)
--- PASS: TestProjectImport_ApplyThenGetNested (6.00s)
--- PASS: TestProjectImport_RejectsScanGroups (integration)
--- PASS: TestProjectImport_OK_NoScanGroupFieldInResponse (integration, 6.00s)
```

Unit import of `{"scan_groups":[],"devices":[]}` → **400** and error names `scan_groups`. Clean nested import → `status=imported`, `devices=1`, `tags=1`, response map has **no** `scan_groups` key. Also: `TestRejectForbiddenScanGroupKeys_*`, `TestLoadBytes_CleanProject_OK`, `TestParseNestedProject_RejectsScanGroups`, `TestLoadGodotBytes_RejectsScanGroups` **PASS**.

---

### Step 3 — Device CRUD + Tag CRUD — **PASS** (working subset + correct HTTP methods)

**Working subset (integration + API):**

```
go test ./tests/integration -count=1 -run "TestListDevices|TestListTags|TestCreateDevice_DuplicateConflict|TestUpdateDevice_NotFound|TestDeleteDevice_NotFound|TestWriteTag_"
go test ./internal/api -count=1 -run "TestCreateTag_|TestAccept_DeviceCRUD_CorrectMethods"
```

| Path | Result |
|------|--------|
| `GET /devices`, `GET /tags` | PASS (`TestListDevices`, `TestListTags`) |
| `POST /devices` create + 409 duplicate | PASS (`TestCreateDevice_DuplicateConflict`) |
| `POST /tags` without `scan_group` → 201 | PASS (`TestCreateTag_NoScanGroupRequired`) |
| `POST /tags` with `scan_group` → 400 naming the key | PASS (`TestCreateTag_RejectsScanGroupKey`) |
| `POST /tags/:name/write` unknown / PLC-down / read-only | PASS (`TestWriteTag_*`) |

**Correct methods probe** (throwaway `TestAccept_DeviceCRUD_CorrectMethods`, then deleted): `POST` create → 201; `GET` → 200; **`PUT /devices/:id`** `{enabled:false}` → 200; **`DELETE /devices/:id`** → 200; subsequent GET → 404. **PASS**.

**Broken `crud_test.go` (not this plan):** update/delete calls use `POST /devices/:id` and `POST /devices/:id/delete`; routes are `PUT` / `DELETE` (`internal/api/http.go`). Hence lifecycle 404. Missing-fields case is stale vs endpoint synthesis.

**Tag update/delete HTTP:** no `PUT`/`DELETE /tags/:name` routes exist (list/create/get/write only). Full tag replace is via `PUT /project` / import. Pre-existing API shape, not a scan_group regression.

---

### Step 4 — CSV round-trip — **PASS**

```
go test ./internal/project -count=1 -run "CSV"
go test ./tests/integration -count=1 -run "TestImportCSV_|TestExportCSV_"
```

```
--- PASS: TestExportDevicesCSV_HeaderAndRows
--- PASS: TestExportTagsCSV_HeaderAndRows
--- PASS: TestImportDevicesCSV_RoundTrip
--- PASS: TestImportTagsCSV_RoundTrip
--- PASS: TestImportDevicesCSV_RejectsScanGroupHeader
--- PASS: TestImportTagsCSV_RejectsScanGroupHeader
--- PASS: TestImportCSV_ThenExport_RoundTrip
--- PASS: TestExportCSV_DefaultProject
```

Headers: devices `Name,Description,Driver,Enabled,ConnectionParams,TagCount`; tags `Device,Address,Name,DataType,Writable,Scale,Unit`. No `ScanGroup` column.

---

### Step 5 — Two devices, different `poll_interval` → 2 tickers — **PASS**

Committed tests:

```
go test ./internal/scheduler ./internal/runtime ./internal/collector -count=1
```

```
--- PASS: TestScheduler_PeriodicCollect   # 3 devices, only 1 enabled-with-tags → started==1
--- PASS: TestScheduler_Stop
--- PASS: TestScheduler_ReloadDuringCollect
--- PASS: TestApplyProject_AfterStartReloadsScheduler  # sch.Active()==1
--- PASS: TestApplyProject_SkipsDisabledDevices
--- PASS: TestCollectDevice_*
```

Throwaway `TestScheduler_TwoDevicesDifferentPollIntervals` (deleted after run): devices `fast` @ 50ms and `slow` @ 200ms, both enabled with tags → `started=2`, `Active()==2`. **PASS** (0.00s).

`Start()` loops every enabled device with ≥1 tag and appends one ticker (`internal/scheduler/scheduler.go`).

---

### Step 6 — Editor: new device, no Scan Group, poll interval, tags, publish — **SKIP** (GUI) / **PASS** (headless + inspection)

GUI launch of create-device / publish not run (headless scripts cover serialize; no interactive editor session).

Code inspection: `industrial_device_form.cpp`, `industrial_new_device_dialog.cpp`, `industrial_device_dock.cpp`, `industrial_tag_dock.cpp` contain **no** `Scan Group` / `scan_group` UI strings. Tuning field `poll_interval` remains (`industrial_device_fields.cpp`, driver schema “Poll Interval (ms)”). Dock grouping is Enabled/Disabled only (`add_status_group`).

Headless persist (covers create→to_dict→save→load, no forbidden keys):

```
bin\godot.windows.editor.x86_64.exe --headless --path app/visualization --script res://tests/test_industrial_project_persistence.gd
```

```
=== test_industrial_project_persistence ===
ERROR: Obsolete key scan_group/scan_groups is not allowed.   # expected reject cases
...
PASS
```

Autoload `res://scripts/Runtime.gd` missing (pre-existing visualization gap); did not fail this script.

---

### Step 7 — Device properties: enabled/params, no Scan Group, create vs property parity — **SKIP** (GUI) / **PASS** (inspection)

Same form/dialog field pipeline (`industrial_device_fields` + driver schema). No extra Scan Group row on the property form vs new-device dialog. Not exercised by launching the editor.

---

### Step 8 — Tag new/batch; tag dock list/delete — **SKIP** (GUI)

No automated GDScript for tag dock / batch-gen dialog. `test_industrial_csv.gd` covers tag CSV import/export without ScanGroup. Batch/new-tag GUI not launched.

---

### Step 9 — Load/publish legacy JSON with `scan_group` → clear error — **PASS**

Covered by `test_industrial_project_persistence.gd` `from_dict` reject cases (root `scan_groups` / `scan_group`, device-level, nested tag). Engine prints:

```
ERROR: Obsolete key scan_group/scan_groups is not allowed.
   at: IndustrialProject::from_dict (modules\industrial_editor\industrial_project.cpp:1031)
```

and `from_dict` returns false without mutating a good project. Same invariant on the Go side (`RejectForbiddenScanGroupKeys`).

---

### Step 10 — Cross-process runtime + editor smoke — **SKIP**

Full empty-runtime → editor register/import → live tag updates → change `poll_interval` → disable device was not started (heavy; no dedicated E2E harness in this environment).

Unit substitutes that **PASS**: `TestApplyProject_AfterStartReloadsScheduler`, `TestApplyProject_SkipsDisabledDevices`, `TestScheduler_PeriodicCollect`, two-device `Active()==2` probe, `TestProjectImport_ApplyThenGetNested`.

---

## Editor extra

```
bin\godot.windows.editor.x86_64.exe --headless --path app/visualization --script res://tests/test_industrial_csv.gd
```

```
=== test_industrial_csv ===
ERROR: Obsolete CSV column ScanGroup is not allowed.   # expected
PASS
```

Widget tests (`test_native_load.gd`, `test_editor_palette.gd`, etc.) were **not** run (out of device/tag scope).

---

## Counts (spec §4 / task-9 steps 1–10)

| Status | Steps |
|--------|--------|
| **PASS** | 2, 3 (subset + PUT/DELETE probe), 4, 5, 9 |
| **FAIL** | 1 (`go test ./...` not green; failures pre-existing) |
| **SKIP** | 6–8 GUI launch (inspection + headless noted above), 10 full cross-process |

Plan-related automated surfaces (import reject, CSV, scheduler/collector/apply, editor persist/CSV) are green.

---

## Concerns

1. Full `go test ./...` remains red for the same integration/modbus reasons as T6, plus `TestSlaveIDAffectsFrameBytes` panic on `Size=0` merge (`fbab98f` lineage) — **not** this plan; not fixed here.
2. `tests/integration/crud_test.go` HTTP verbs do not match Gin routes (`POST` vs `PUT`/`DELETE`).
3. No HTTP tag update/delete; editor still has unused `IndustrialTagData.poll_interval` (spec asked to drop tag-level poll clock). Neither is a `scan_group` leak.
4. Step 10 E2E (live collect rate after re-import / disable) was not run against a real runtime+editor pair.
