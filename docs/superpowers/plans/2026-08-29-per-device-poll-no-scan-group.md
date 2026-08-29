# Per-device Poll (No Scan Group) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Align collection with EBPro—one `poll_interval` per device, all of that device’s tags collected together—and completely remove `scan_group` / `scan_groups` from backend, API, CSV, and Godot editor.

**Architecture:** Scheduler ticks per enabled device (interval = `DeviceDef.PollIntervalMs`). Collector exposes `CollectDevice`. Project load/import hard-rejects any JSON/CSV that still contains `scan_group` or `scan_groups`. Editor drops Scan Group UI and never serializes those keys.

**Tech Stack:** Go 1.22+ (`driver-engine`), Gin HTTP, Godot 4.x C++ module `industrial_editor` in `dt-engine` worktree `feat/project-import-tag-schema`.

**Spec:** `docs/superpowers/specs/2026-08-29-per-device-poll-no-scan-group-design.md` (same path in both repos)

## Global Constraints

- `scan_group` / `scan_groups` must **never** appear in valid configs; presence → hard error (no silent strip).
- Sole clock: `DeviceDef.poll_interval` (default 1000 ms if unset at schedule time; floor `project.MinScanIntervalMs`).
- Work in: `D:\driver-engine` and `D:\dt-engine\.worktrees\project-import-tag-schema` on branch `feat/project-import-tag-schema`.
- Do not pop unrelated git stashes; do not edit `D:\dt-engine\modules\...` main checkout for this work.
- Cross-platform editor code: no Win32/STL in module changes.

## File map

| Area | Files |
|------|--------|
| Reject keys + model | `internal/project/loader.go`, new `internal/project/forbidden_keys.go`, tests |
| Formats | `nested_format.go`, `godot_format.go`, `csv.go` (+ tests) |
| Tags registry | `internal/tags/registry.go` |
| Collector / scheduler | `internal/collector/collector.go`, `internal/scheduler/scheduler.go` (+ tests) |
| Apply / API | `internal/runtime/runtime.go`, `internal/api/http.go`, `internal/api/project_import.go` |
| Fixtures | `examples/project.json`, integration tests, Postman |
| Editor | `industrial_project.{h,cpp}`, `industrial_device_form.{h,cpp}`, `industrial_device_dock.{h,cpp}`, `industrial_csv_io.cpp`, `industrial_new_device_dialog.cpp`, `industrial_batch_gen_dialog.cpp` |

---

### Task 1: Forbidden-key reject + strip scan group from Project model

**Files:**
- Create: `internal/project/forbidden_keys.go`
- Create: `internal/project/forbidden_keys_test.go`
- Modify: `internal/project/loader.go` (remove `ScanGroups`, `ScanGroupDef`, `TagDef.ScanGroup`, `TagDef.PollIntervalMs`; slim `Validate` / `Normalize`; call reject from `LoadBytes`)
- Modify: `internal/project/loader_test.go`

**Interfaces:**
- Produces: `func RejectForbiddenScanGroupKeys(data []byte) error`
- Produces: `Project` without `ScanGroups`; `TagDef` without `ScanGroup` / `PollIntervalMs`
- Produces: `Normalize()` no longer creates groups; may default `DeviceDef.PollIntervalMs` to 1000 when 0
- Removes: `TagsByGroup`, `ScanGroupByName`

- [ ] **Step 1: Write failing tests for forbidden keys**

```go
func TestRejectForbiddenScanGroupKeys_Root(t *testing.T) {
	err := RejectForbiddenScanGroupKeys([]byte(`{"scan_groups":[],"devices":[],"tags":[]}`))
	if err == nil {
		t.Fatal("expected error")
	}
}

func TestRejectForbiddenScanGroupKeys_TagField(t *testing.T) {
	err := RejectForbiddenScanGroupKeys([]byte(`{
	  "devices":[{"id":"d","name":"D","driver":"siemens_s7","endpoint":"h:1","enabled":true}],
	  "tags":[{"name":"T","device_id":"d","address":"M10","data_type":"int16","scan_group":"fast"}]
	}`))
	if err == nil {
		t.Fatal("expected error")
	}
}

func TestLoadBytes_CleanProject_OK(t *testing.T) {
	raw := []byte(`{
	  "devices":[{"id":"d","name":"D","driver":"siemens_s7","endpoint":"h:1","enabled":true,"poll_interval":200}],
	  "tags":[{"name":"T","device_id":"d","address":"M10","data_type":"int16"}]
	}`)
	p, err := LoadBytes(raw)
	if err != nil {
		t.Fatal(err)
	}
	if p.Devices[0].PollIntervalMs != 200 {
		t.Fatalf("poll_interval=%d", p.Devices[0].PollIntervalMs)
	}
}
```

- [ ] **Step 2: Run tests — expect fail (RejectForbidden… undefined)**

Run: `go test ./internal/project/ -run "RejectForbidden|LoadBytes_Clean" -count=1`

- [ ] **Step 3: Implement reject helper + model surgery**

`forbidden_keys.go`: unmarshal into `map[string]json.RawMessage`; if key `scan_groups` or `scan_group` at root → error; walk `devices` / `tags` arrays as `[]map[string]json.RawMessage` and reject those keys on each object.

`LoadBytes`: call `RejectForbiddenScanGroupKeys(data)` before decode.

Remove `ScanGroups` / `ScanGroupDef` / tag scan+poll fields; delete group validation and `Normalize` group generation; default device `PollIntervalMs` to 1000 when 0 inside `Normalize`. Delete `TagsByGroup` / `ScanGroupByName`.

Rewrite `loader_test.go` fixtures: no `scan_groups` / `scan_group`; delete tests that only covered ScanGroupDef unmarshal / group derivation; keep device/tag integrity tests.

- [ ] **Step 4: Run package tests**

Run: `go test ./internal/project/ -count=1`  
Expected: PASS (nested/godot/csv may still fail until later tasks—if so, temporarily skip only if compile errors; prefer fixing compile by stubbing callers in same commit wave).

If `nested_format.go` etc. still reference removed types, fix compile in Task 1 by removing those fields from format structs enough to compile, or land Task 2 in the same commit. Prefer one commit that compiles.

- [ ] **Step 5: Commit**

```bash
git add internal/project/forbidden_keys.go internal/project/forbidden_keys_test.go internal/project/loader.go internal/project/loader_test.go
# plus any format files touched only to compile
git commit -m "feat(project): reject scan_group keys; drop ScanGroup from model"
```

---

### Task 2: nested / godot / csv formats without scan groups

**Files:**
- Modify: `internal/project/nested_format.go`, `nested_format_test.go`
- Modify: `internal/project/godot_format.go`, `godot_format_test.go`
- Modify: `internal/project/csv.go`, `csv_test.go`

**Interfaces:**
- Consumes: `RejectForbiddenScanGroupKeys` on raw nested/godot JSON before parse
- Produces: CSV headers without `ScanGroup`:
  - Devices: `Name,Description,Driver,Enabled,ConnectionParams,TagCount`
  - Tags: `Device,Address,Name,DataType,Writable,Scale,Unit`
- Produces: `ImportDevicesCSV(r) ([]DeviceDef, error)` — no `[]ScanGroupDef` return

- [ ] **Step 1: Rewrite failing format tests**

Replace `TestParseNestedProject_ScanGroupEnabledDefault` with:

```go
func TestParseNestedProject_RejectsScanGroups(t *testing.T) {
	_, err := ParseNestedProject([]byte(`{"scan_groups":[{"name":"fast","interval_ms":200}],"devices":[]}`))
	if err == nil {
		t.Fatal("expected reject")
	}
}
```

Update godot/csv tests: fixtures without scan group; assert headers; ImportDevicesCSV returns only devices.

- [ ] **Step 2: Run — expect fail on reject / header mismatch**

Run: `go test ./internal/project/ -run "Nested|Godot|CSV|csv" -count=1`

- [ ] **Step 3: Implement format cleanup**

Remove all `ScanGroups` / `ScanGroup` from nested/godot structs and convert paths. Call `RejectForbiddenScanGroupKeys` at entry of `ParseNestedProject` / Godot parse. CSV: drop column; reject import if header contains `ScanGroup` (exact column name).

- [ ] **Step 4: `go test ./internal/project/ -count=1` → PASS**

- [ ] **Step 5: Commit**

```bash
git add internal/project/nested_format.go internal/project/nested_format_test.go \
  internal/project/godot_format.go internal/project/godot_format_test.go \
  internal/project/csv.go internal/project/csv_test.go
git commit -m "feat(project): strip scan_group from nested/godot/csv formats"
```

---

### Task 3: tags.Registry — remove ScanGroup index

**Files:**
- Modify: `internal/tags/registry.go`
- Modify: any `internal/tags/*_test.go` if present
- Modify callers that set `ScanGroup` (runtime apply) as needed for compile

**Interfaces:**
- Produces: `tags.Def` without `ScanGroup`
- Produces: keep `NamesByDevice(deviceID string) []string`
- Removes: `byGroup`, `NamesByGroup`

- [ ] **Step 1: Write/adjust test that Add + NamesByDevice works without ScanGroup**

```go
func TestRegistry_NamesByDevice(t *testing.T) {
	r := New(nil)
	_ = r.Add(Def{Name: "t0", DeviceID: "d1", Address: "M0", DataType: "int16"})
	names := r.NamesByDevice("d1")
	if len(names) != 1 || names[0] != "t0" {
		t.Fatalf("%v", names)
	}
}
```

- [ ] **Step 2: Run test**

- [ ] **Step 3: Remove ScanGroup from Def and byGroup map maintenance**

- [ ] **Step 4: `go test ./internal/tags/ ./internal/runtime/ -count=1` (fix compile callers)**

- [ ] **Step 5: Commit**

```bash
git commit -am "refactor(tags): drop ScanGroup from registry Def"
```

---

### Task 4: Collector `CollectDevice`

**Files:**
- Modify: `internal/collector/collector.go`
- Modify: `internal/collector/collector_test.go`

**Interfaces:**
- Produces: `func (c *Collector) CollectDevice(ctx context.Context, deviceID string) error`
- Removes: `CollectGroup`
- Consumes: `tagReg.NamesByDevice(deviceID)`

- [ ] **Step 1: Rename/adapt tests to CollectDevice**

Change `TestCollectGroup_*` → `TestCollectDevice_*`; register tags without `ScanGroup`; call `CollectDevice(ctx, "dev1")`. Keep batching assertions identical.

- [ ] **Step 2: Run — expect fail (CollectDevice undefined)**

Run: `go test ./internal/collector/ -count=1`

- [ ] **Step 3: Implement CollectDevice**

Copy CollectGroup body; source names via `NamesByDevice`; log field `device_id` instead of `scan_group`. Delete CollectGroup.

- [ ] **Step 4: Tests PASS**

- [ ] **Step 5: Commit**

```bash
git commit -am "feat(collector): CollectDevice replaces CollectGroup"
```

---

### Task 5: Scheduler per-device tickers

**Files:**
- Modify: `internal/scheduler/scheduler.go`
- Modify: `internal/scheduler/scheduler_test.go`
- Modify: `internal/runtime/apply_project_test.go` fixtures (no ScanGroups)

**Interfaces:**
- Consumes: `proj.Devices`, `tagReg.NamesByDevice`, `coll.CollectDevice`
- Produces: one ticker per device where `Enabled && len(NamesByDevice(id))>0`; interval = `PollIntervalMs` or 1000; if below `MinScanIntervalMs`, use floor

- [ ] **Step 1: Rewrite scheduler tests**

Project fixture:

```go
Devices: []project.DeviceDef{{
  ID: "dev1", Name: "D", Driver: "mock", Endpoint: "x", Enabled: true, PollIntervalMs: 50,
}},
Tags: []project.TagDef{{Name: "t0", DeviceID: "dev1", Address: "DB1.DBW0", DataType: "int16"}},
```

Assert `CollectDevice` called (mock collector or counting stub used by existing tests—adapt to deviceID).

- [ ] **Step 2: Run — fail on old ScanGroups API**

- [ ] **Step 3: Implement Start loop over devices**

```go
for _, d := range s.proj.Devices {
  if !d.Enabled { continue }
  if len(s.tagReg.NamesByDevice(d.ID)) == 0 { continue }
  ms := d.PollIntervalMs
  if ms <= 0 { ms = 1000 }
  if ms < project.MinScanIntervalMs { ms = project.MinScanIntervalMs }
  ticker := time.NewTicker(time.Duration(ms) * time.Millisecond)
  // runDevice(ctx, d.ID, ticker) calling CollectDevice
}
```

Update package comment. Fix apply_project_test fixtures.

- [ ] **Step 4: `go test ./internal/scheduler/ ./internal/runtime/ -count=1` → PASS**

- [ ] **Step 5: Commit**

```bash
git commit -am "feat(scheduler): tick per device poll_interval"
```

---

### Task 6: HTTP API + import response + examples

**Files:**
- Modify: `internal/api/http.go` (tag create body; CSV import path; any scan_group JSON)
- Modify: `internal/api/project_import.go`
- Modify: `examples/project.json`
- Modify: `docs/industrial-runtime-api.postman_collection.json` (if present)
- Modify: `tests/integration/*.go` fixtures
- Modify: `cmd/demo/main.go` if it sets ScanGroup

**Interfaces:**
- Tag create JSON: no `scan_group` field
- Import JSON response: `{"status":"imported","devices":N,"tags":M,"applied":...}` — **no** `scan_groups`
- Import/load path already rejects forbidden keys via project parsers

- [ ] **Step 1: Add API test (or extend project_import_test)**

```go
func TestProjectImport_RejectsScanGroups(t *testing.T) {
  // POST body with "scan_groups":[] → expect 400
}
func TestProjectImport_OK_NoScanGroupFieldInResponse(t *testing.T) {
  // assert response map has no key scan_groups
}
```

- [ ] **Step 2: Run integration/API test — fail**

- [ ] **Step 3: Remove ScanGroup from createTagReq; fix import response; rewrite examples/project.json**

Example device must include `"poll_interval": 200` (or similar); tags omit `scan_group`; delete root `scan_groups` array.

Update all integration fixtures the same way.

- [ ] **Step 4: `go test ./internal/api/ ./tests/integration/ -count=1` → PASS** (skip live PLC tests if env missing)

- [ ] **Step 5: Commit**

```bash
git add examples/project.json internal/api/ docs/industrial-runtime-api.postman_collection.json tests/integration/ cmd/demo/
git commit -m "feat(api): drop scan_group from tags/import; refresh examples"
```

---

### Task 7: Editor — IndustrialProject model & serialize

**Files (worktree):**
- Modify: `modules/industrial_editor/industrial_project.h`
- Modify: `modules/industrial_editor/industrial_project.cpp`
- Modify: `modules/industrial_editor/industrial_new_device_dialog.cpp` (remove `dev.scan_group = ""`)
- Modify: `modules/industrial_editor/industrial_batch_gen_dialog.cpp`

**Interfaces:**
- Remove: `IndustrialScanGroup`, `scan_groups` vector, device/tag `scan_group` fields, CRUD methods for scan groups
- `to_dict` / `from_dict`: never read/write `scan_group`/`scan_groups`; if `from_dict` sees those keys on Dictionary → append validate error or fail load (match spec hard-reject)
- Validate: remove duplicate scan-group / missing scan-group reference checks

- [ ] **Step 1: Add/extend GDScript or C++-side persistence test if present (`test_industrial_project_persistence.gd`); else document manual check**

Prefer: unit-style check in existing persistence test that round-trip dict has no `scan_groups` key.

- [ ] **Step 2: Strip fields/methods from .h/.cpp; reject keys in `from_dict`**

```cpp
if (p_data.has("scan_groups") || p_data.has("scan_group")) {
  errors.append(TTR("Obsolete key scan_group/scan_groups is not allowed."));
  return false; // or accumulate then fail
}
```

Same for each device/tag Dictionary when loading nested arrays.

- [ ] **Step 3: Build industrial_editor objects**

Run: `scons platform=windows target=editor module_industrial_editor_enabled=yes -j16` from worktree  
Expected: compile success for touched TUs

- [ ] **Step 4: Commit**

```bash
git add modules/industrial_editor/industrial_project.h modules/industrial_editor/industrial_project.cpp \
  modules/industrial_editor/industrial_new_device_dialog.cpp modules/industrial_editor/industrial_batch_gen_dialog.cpp
git commit -m "feat(industrial_editor): remove scan_group from project model"
```

---

### Task 8: Editor — device form, dock, CSV

**Files:**
- Modify: `industrial_device_form.h`, `industrial_device_form.cpp` — remove Scan Group row, `field_scan_group`, refresh/`_on_scan_group_changed`
- Modify: `industrial_device_dock.h`, `industrial_device_dock.cpp` — remove `VIEW_BY_GROUP` or repurpose to non-scan grouping; delete scan_group branching
- Modify: `industrial_csv_io.cpp` — headers without ScanGroup; reject/import without column

- [ ] **Step 1: Remove UI + dock mode; fix CSV**

Device CSV: `Name,Description,Driver,Enabled,ConnectionParams,TagCount`  
Tag CSV: `Device,Address,Name,DataType,Writable,Scale,Unit`

- [ ] **Step 2: Rebuild editor**

- [ ] **Step 3: Smoke manually (or E2E if available): open device form — no Scan Group; publish JSON has no forbidden keys**

- [ ] **Step 4: Commit**

```bash
git commit -am "feat(industrial_editor): drop Scan Group UI and CSV columns"
```

---

### Task 9: Full device + tag acceptance retest (spec §4)

**Files:** none required except fix bugs found  
**Workdirs:** both repos

**Checklist (executor must run and record results in commit message or `docs/superpowers/.../acceptance-log.md`):**

**Backend**
- [ ] **Step 1:** `go test ./...` in `D:\driver-engine` (exclude or skip live PLC if unreachable)
- [ ] **Step 2:** Import body with `scan_groups` → 400; clean import → OK; response has no `scan_groups`
- [ ] **Step 3:** Device CRUD + Tag CRUD via HTTP (or `tests/integration/crud_test.go`)
- [ ] **Step 4:** CSV round-trip `csv_interop_test.go`
- [ ] **Step 5:** Apply/reload: two devices different `poll_interval` — verify scheduler starts 2 tickers (unit or log)

**Editor**
- [ ] **Step 6:** Launch worktree `bin\godot.windows.editor.x86_64.exe`; create device; confirm no Scan Group; set poll interval; add tags; publish
- [ ] **Step 7:** Device properties: edit enabled/params; still no Scan Group; create vs property field parity (no +1 scan group)
- [ ] **Step 8:** Tag new/batch; tag dock list/delete
- [ ] **Step 9:** Attempt load/publish legacy JSON with `scan_group` → clear error

**Cross-process**
- [ ] **Step 10:** Runtime empty start → editor register + import → tags update; change one device poll_interval → re-import → rate changes; disable device → collect stops

- [ ] **Step 11: Commit acceptance log if created**

```bash
git commit -am "test: per-device poll acceptance for device/tag management"
```

---

## Self-review (author)

| Spec requirement | Task |
|------------------|------|
| Per-device poll clock | 5 |
| CollectDevice / all tags on device | 4 |
| Hard-reject scan_group keys | 1, 2, 6, 7 |
| Remove model/API/CSV/editor scan group | 1–3, 6–8 |
| No silent strip | 1 (reject only) |
| Full device+tag FE/BE retest | 9 |
| examples/fixtures updated | 6 |

No TBD/placeholder steps remain. Types: `CollectDevice(ctx, deviceID string) error`; `RejectForbiddenScanGroupKeys([]byte) error`; CSV without ScanGroup column.

---

## Execution handoff

Plan complete and saved to:

- `D:\driver-engine\docs\superpowers\plans\2026-08-29-per-device-poll-no-scan-group.md`
- (mirror) `D:\dt-engine\.worktrees\project-import-tag-schema\docs\superpowers\plans\2026-08-29-per-device-poll-no-scan-group.md`

**Two execution options:**

1. **Subagent-Driven (recommended)** — fresh subagent per task, review between tasks  
2. **Inline Execution** — executing-plans in this session with checkpoints  

Which approach?
