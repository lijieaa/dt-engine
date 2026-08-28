# Project Import + Dual Tag Schema Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver `POST /api/v1/project/import` with EBPro-aligned device fields and absolute/symbolic tag schemas, then wire the dt-engine editor to serialize and publish that JSON (S7 Ethernet + Modbus TCP first).

**Architecture:** Backend owns catalog keys, `DeviceDef` flat+`options`, dual `TagDef` fields, address derivation, and hot-apply. Editor stores the same nested shape in `project.json`, rebuilds device/tag dialogs into EBPro groups/modes, and calls import after project registration. No `godot` in new public names.

**Tech Stack:** Go 1.22+ (`driver-engine`: Gin, `internal/project`, `internal/api`, `internal/runtime`); Godot 4.x C++ module (`dt-engine/modules/industrial_editor`); HTTP JSON between them.

**Spec:** `docs/superpowers/specs/2026-08-28-project-import-tag-schema-design.md` (both repos)

## Global Constraints

- Public routes/identifiers: **no** substring `godot` (use `nested` / `ImportNested` / `import`).
- Device `driver`: **string** `driver_key` only on the wire.
- Tag absolute: `address_mode` / `address_type` / `data_format` / offset `address`; symbolic: `symbol`.
- `data_format` ids: catalog (`i16`, `f32`, `bit`, …).
- `scan_groups`: optional; always run `Normalize()` after import.
- Delivery adapters: S7 Ethernet family keys + `modbus_tcp`; symbolic schema validated even if adapter missing → clear error.
- Repos: implement Tasks 1–6 in `D:\driver-engine`, Tasks 7–10 in `D:\dt-engine` (commits in the repo being edited).

---

## File map

### driver-engine

| File | Role |
|------|------|
| `internal/project/loader.go` | Extend `TagDef`; keep `DeviceDef` |
| `internal/project/nested_format.go` | **Create** — parse/export nested import JSON (no godot name) |
| `internal/project/address_derive.go` | **Create** — absolute/symbolic → raw address + value kind |
| `internal/project/nested_format_test.go` | **Create** |
| `internal/project/address_derive_test.go` | **Create** |
| `internal/api/driver_catalog.go` | Hygiene: `use_udp`, `comm_delay`, S7 `max_pdu_size` |
| `internal/api/project_import.go` | **Create** — HTTP handler |
| `internal/api/http.go` | Register route; `format=nested` on GET |
| `internal/runtime/runtime.go` | `ApplyProject` hot reload |
| `tests/integration/project_import_test.go` | **Create** |

### dt-engine

| File | Role |
|------|------|
| `modules/industrial_editor/industrial_project.h/.cpp` | Device/tag structs + `to_dict`/`from_dict` |
| `modules/industrial_editor/industrial_new_device_dialog.*` | §A–§D UI |
| `modules/industrial_editor/industrial_device_form.*` | Same grouping |
| `modules/industrial_editor/industrial_new_tag_dialog.*` | Absolute vs symbolic UI |
| `modules/industrial_editor/industrial_runtime_client.h/.cpp` | `import_project` |
| `modules/industrial_editor/industrial_editor_plugin.cpp` | Publish / post-create import |

---

### Task 1: Catalog hygiene (driver-engine)

**Files:**
- Modify: `internal/api/driver_catalog.go` (`commonFieldsEth` / S7 ethernet extras / ensure tuning block)

**Produces:** `use_udp` default `false` (bool); shared tuning keys include `comm_delay`; S7 Ethernet extras include `max_pdu_size` (int, default 240).

- [ ] **Step 1: Fix `use_udp` default**

In `commonFieldsEth`, change `Use UDP` field `DefaultVal` from `"device"` to `false` and `Kind` to `kindBool` (or `kindChoice` with bool-compatible defaults matching other bools in catalog — prefer `kindBool` + `false`).

- [ ] **Step 2: Ensure tuning keys on every composed list**

Add to `commonFieldsA` (or a `commonFieldsTuning` appended by `ethernetFields` / `serialFields` / `freeFields`):

```go
{LabelMsgID: "Comm Delay (ms)", Key: "comm_delay", Kind: kindInt, DefaultVal: 0, MinVal: 0, MaxVal: 60000},
```

If `timeout` / `retries` / `poll_interval` already in A, do not duplicate; only add missing `comm_delay`. Ensure Eth block still has `max_read_words`, `max_write_words`, `block_size_words`.

- [ ] **Step 3: Add `max_pdu_size` to S7 Ethernet extras**

For indexes 0–3 S7 Ethernet `ethernetFields(...)` extras, append:

```go
driverField{LabelMsgID: "Max PDU Size", Key: "max_pdu_size", Kind: kindInt, DefaultVal: 240, MinVal: 64, MaxVal: 960},
```

- [ ] **Step 4: Commit**

```bash
cd D:\driver-engine
git add internal/api/driver_catalog.go
git commit -m "fix(api): align driver catalog defaults with DeviceDef and S7 options"
```

---

### Task 2: Extend TagDef + address derivation (driver-engine)

**Files:**
- Modify: `internal/project/loader.go` (`TagDef` struct)
- Create: `internal/project/address_derive.go`
- Create: `internal/project/address_derive_test.go`

**Produces:**
- `TagDef` fields: `Schema`, `Description`, `AddressMode`, `AddressType`, `DataFormat`, `DBNumber`, `Length`, `Symbol` (plus existing `Name`, `DeviceID`, `Address` derived, `DataType` derived, `Writable`, `Scale`, `PollIntervalMs`, `ScanGroup`)
- `func DeriveTagAddress(driverKey string, t *TagDef) error` — fills `Address` + `DataType` from absolute/symbolic fields

- [ ] **Step 1: Write failing tests**

```go
// internal/project/address_derive_test.go
func TestDeriveTagAddress_S7_DBWord(t *testing.T) {
	tag := &TagDef{
		Schema: "absolute", AddressMode: "word", AddressType: "DBn",
		DBNumber: 1, Address: "0", DataFormat: "i16",
	}
	if err := DeriveTagAddress("siemens_s7", tag); err != nil {
		t.Fatal(err)
	}
	if tag.Address != "DB1.DBW0" && tag.Address != "DBn 1 0" {
		// Accept the canonical form chosen in implementation; document in comment.
		// Prefer EBPro-style used by s7.ParseAddress — assert against real parser.
	}
	if tag.DataType != "int16" && tag.DataType != "i16" {
		t.Fatalf("DataType=%q", tag.DataType)
	}
}

func TestDeriveTagAddress_ModbusCoil(t *testing.T) {
	tag := &TagDef{
		Schema: "absolute", AddressMode: "bit", AddressType: "0x_Coil",
		Address: "5", DataFormat: "bit",
	}
	if err := DeriveTagAddress("modbus_tcp", tag); err != nil {
		t.Fatal(err)
	}
	// Must be parseable by modbus.ParseAddress
	if _, err := modbus.ParseAddress(tag.Address); err != nil {
		t.Fatal(err)
	}
}

func TestDeriveTagAddress_Symbolic(t *testing.T) {
	tag := &TagDef{Schema: "symbolic", Symbol: "Program:Main.T"}
	if err := DeriveTagAddress("ab_controllogix_eip_class3_tag", tag); err != nil {
		t.Fatal(err)
	}
	if tag.Address != "Program:Main.T" {
		t.Fatalf("Address=%q", tag.Address)
	}
}
```

Adjust expected raw strings after checking `s7.ParseAddress` / `modbus.ParseAddress` accepted forms in existing tests.

- [ ] **Step 2: Run tests — expect fail**

```bash
cd D:\driver-engine
go test ./internal/project/ -run DeriveTagAddress -v
```

- [ ] **Step 3: Implement `TagDef` extensions + `DeriveTagAddress`**

Map `data_format` → internal `DataType` string used by registry today:

| catalog | DataType (internal) |
|---------|---------------------|
| `i16` | `int16` |
| `u16` | `uint16` |
| `i32` | `int32` |
| `u32` | `uint32` |
| `u8` | `byte` |
| `f32` / `real32` | `float` or `float32` (match s7 ValueKind) |
| `f64` / `real64` | `double` |
| `bcd16` / `bcd32` | `bcd16` / `bcd32` |
| `bit` | `bool` |
| `str_a` / `str_w` | `string` / keep as needed |

Absolute S7: build raw from `address_type` + `db_number` + `address` (inspect existing `s7` address examples). Absolute Modbus: `"0x Coil "+offset`, `"4x_HR "+offset`, etc. from `address_type` id.

- [ ] **Step 4: Run tests — pass**

```bash
go test ./internal/project/ -run DeriveTagAddress -v
```

- [ ] **Step 5: Commit**

```bash
git add internal/project/loader.go internal/project/address_derive.go internal/project/address_derive_test.go
git commit -m "feat(project): dual tag fields and address derivation"
```

---

### Task 3: Nested import/export parser (driver-engine)

**Files:**
- Create: `internal/project/nested_format.go`
- Create: `internal/project/nested_format_test.go`

**Produces:**
- `func ParseNestedProject(data []byte) (*Project, error)`
- `func ExportNestedProject(p *Project) ([]byte, error)`

**Consumes:** `DeriveTagAddress`, `DeviceDef`, `Normalize`, `Validate`

- [ ] **Step 1: Failing test — S7 + Modbus nested body**

```go
const sample = `{
  "devices": [{
    "name": "plc", "driver": "siemens_s7", "enabled": true,
    "ip": "192.168.0.10", "port": 102,
    "poll_interval": 500, "timeout": 1000,
    "options": {"rack": 0, "slot": 1},
    "tags": [{
      "schema": "absolute", "name": "Speed",
      "address_mode": "word", "address_type": "DBn",
      "db_number": 1, "address": "0", "data_format": "i16"
    }]
  },{
    "name": "mb", "driver": "modbus_tcp", "enabled": true,
    "ip": "192.168.0.20", "port": 502,
    "options": {"slave_id": 1},
    "tags": [{
      "schema": "absolute", "name": "C1",
      "address_mode": "bit", "address_type": "0x_Coil",
      "address": "0", "data_format": "bit", "writable": true
    }]
  }]
}`

func TestParseNestedProject_S7AndModbus(t *testing.T) {
	p, err := ParseNestedProject([]byte(sample))
	if err != nil { t.Fatal(err) }
	if len(p.Devices) != 2 || len(p.Tags) != 2 { t.Fatalf("%d %d", len(p.Devices), len(p.Tags)) }
	if p.Devices[0].Driver != "siemens_s7" { t.Fatal(p.Devices[0].Driver) }
	if p.Tags[0].Address == "" { t.Fatal("expected derived address") }
	if len(p.ScanGroups) == 0 { t.Fatal("Normalize should create scan groups") }
}
```

- [ ] **Step 2: Implement parser**

- Unmarshal nested devices; set `DeviceDef.ID = name` if empty; copy flat fields; merge `options` into `Options`.
- For each tag: set `DeviceID`, call `DeriveTagAddress(driver, tag)`, append to flat `p.Tags`.
- Infer `schema` from driver catalog addressing mode if omitted (optional helper; can hardcode: if `Symbol != ""` → symbolic).
- Reject unknown driver strings with clear error.
- Compat: if device has only `connection_params` map, lift known DeviceDef keys to top-level and rest to `Options`.
- End with `p.Normalize()` then `p.Validate()` (order: derive addresses before Validate needs Address/DataType).

- [ ] **Step 3: ExportNestedProject** — reverse nesting tags by `DeviceID`; emit dual-schema fields from TagDef (prefer stored schema fields over reverse-parsing Address when present).

- [ ] **Step 4: Tests pass + commit**

```bash
go test ./internal/project/ -run Nested -v
git add internal/project/nested_format.go internal/project/nested_format_test.go
git commit -m "feat(project): nested project import/export format"
```

---

### Task 4: ApplyProject hot path (driver-engine)

**Files:**
- Modify: `internal/runtime/runtime.go`
- Test: unit or integration after Task 5

**Produces:** `func (rt *Runtime) ApplyProject(p *project.Project) error`

- [ ] **Step 1: Implement ApplyProject**

Behavior:

1. `rt.SetProject(p)`
2. Stop/remove existing devices that are not in the new set (or stop all and clear registries — pick simplest correct approach: stop all managed devices, clear tag registry entries, then re-add).
3. For each enabled device in `p.Devices`: `BuildConfig()` → `devMgr.Add` → `Start`.
4. For each tag: `tagReg.Add` with derived address/datatype/scangroup.
5. Restart or rebuild scheduler from `p` (mirror `New` startup loop for tags/devices — read existing `New` and extract shared helper if needed).

Document in comment if full zero-downtime is not required: brief interruption OK for v1.

- [ ] **Step 2: Commit**

```bash
git add internal/runtime/runtime.go
git commit -m "feat(runtime): ApplyProject reloads devices tags and scheduler"
```

---

### Task 5: HTTP `POST /api/v1/project/import` + GET nested (driver-engine)

**Files:**
- Create: `internal/api/project_import.go`
- Modify: `internal/api/http.go`
- Modify: `getProject` for `format=nested`
- Create: `tests/integration/project_import_test.go`

**Produces:** route registered; handler calls `ParseNestedProject` + optional `ApplyProject`

- [ ] **Step 1: Handler**

```go
func (s *Server) importProject(c *gin.Context) {
	body, err := io.ReadAll(c.Request.Body)
	// parse apply query/json field default true
	p, err := project.ParseNestedProject(body)
	// merge project_id/name from body if present
	s.store.SetProject(p)
	if apply {
		if applier, ok := s.store.(interface{ ApplyProject(*project.Project) error }); ok {
			if err := applier.ApplyProject(p); err != nil { /* 500 */ }
		}
	}
	c.JSON(200, gin.H{"status":"imported","devices":len(p.Devices),"tags":len(p.Tags),"scan_groups":len(p.ScanGroups),"applied":apply})
}
```

Prefer typing `ApplyProject` on an interface next to `ProjectStore` rather than blind cast.

- [ ] **Step 2: Register** `v1.POST("/project/import", s.importProject)`

- [ ] **Step 3: GET** `format=nested` → `ExportNestedProject`; keep `format=godot` as temporary alias calling same exporter or old path.

- [ ] **Step 4: Integration test** using existing test harness (`tests/integration`) posting sample from Task 3; assert GET project / tags.

```bash
cd D:\driver-engine
go test ./tests/integration/ -run ProjectImport -v
```

- [ ] **Step 5: Commit**

```bash
git add internal/api/project_import.go internal/api/http.go tests/integration/project_import_test.go
git commit -m "feat(api): POST /project/import and nested project export"
```

---

### Task 6: Backend self-check vs spec

- [ ] **Step 1: Manual checklist**

- [ ] No new symbol named `*Godot*` in import path  
- [ ] S7 + Modbus TCP import with `apply=true`  
- [ ] Empty `scan_groups` in body still yields groups after Normalize  
- [ ] Symbolic tag on absolute driver → 400  

- [ ] **Step 2: Commit any fixes**

---

### Task 7: Editor data model serialize (dt-engine)

**Files:**
- Modify: `modules/industrial_editor/industrial_project.h`
- Modify: `modules/industrial_editor/industrial_project.cpp`

**Produces:** structs matching §3 / §4 of spec; `to_dict`/`from_dict` emit flat device + `options` + dual tags; compat read of old `connection_params` / int `data_type`.

- [ ] **Step 1: Extend structs**

`IndustrialDeviceData`: add first-class fields (`dev_type`, `location_mode`, `ip`, `port`, …) + `Dictionary options`; keep `connection_params` only as deprecated merge helper during load.

`IndustrialTagData`: add `schema`, `address_mode`, `address_type`, `data_format`, `db_number`, `length`, `symbol`, `description`; keep legacy `data_type` int for compat read.

Store `driver` as **String** key on the wire; UI may still use int index internally — on save convert via `industrial_get_driver_key(idx)` / reverse lookup.

- [ ] **Step 2: Rewrite `to_dict` / `from_dict`** to nested shape in the spec (devices[].tags[] absolute/symbolic).

- [ ] **Step 3: Headless or existing persistence test**

If `app/visualization/tests/test_industrial_project_persistence.gd` exists, extend expectations; else add a small C++/GDScript check that round-trips JSON.

- [ ] **Step 4: Commit**

```bash
cd D:\dt-engine
git add modules/industrial_editor/industrial_project.h modules/industrial_editor/industrial_project.cpp
git commit -m "feat(industrial_editor): project JSON matches nested import schema"
```

---

### Task 8: Device dialog §A–§D (dt-engine)

**Files:**
- Modify: `industrial_new_device_dialog.cpp/.h`
- Modify: `industrial_device_form.cpp/.h`

**Produces:** UI sections Common / Interface / Protocol / Tuning; collect into flat fields + `options`.

- [ ] **Step 1: Section containers** — four `VBox`/`FoldableContainer` labeled to match EBPro groups.

- [ ] **Step 2: `_rebuild_params`** — place catalog fields into sections by key sets (common / eth|serial / tuning / else→protocol).

- [ ] **Step 3: Save path** writes `IndustrialDeviceData` flat+`options`, not only `connection_params`.

- [ ] **Step 4: Commit**

```bash
git commit -m "feat(industrial_editor): EBPro-grouped device property UI"
```

---

### Task 9: Tag dialog absolute / symbolic (dt-engine)

**Files:**
- Modify: `industrial_new_tag_dialog.cpp/.h` (and edit paths if separate)

**Produces:** layout switches on device driver’s `addressing_mode` from runtime catalog / meta.

- [ ] **Step 1: Absolute mode** — Bit/Word, address type, data format (hide format when Bit → force `bit`), offset, conditional DB/length.

- [ ] **Step 2: Symbolic mode** — symbol LineEdit; optional data_format; hide absolute controls.

- [ ] **Step 3: Persist** into extended `IndustrialTagData`.

- [ ] **Step 4: Commit**

```bash
git commit -m "feat(industrial_editor): EBPro absolute and symbolic tag dialog"
```

---

### Task 10: Runtime client import + publish (dt-engine)

**Files:**
- Modify: `industrial_runtime_client.h/.cpp`
- Modify: `industrial_editor_plugin.cpp`

**Produces:** `IndustrialRuntimeClient::import_project(const String &json_body, Node *owner)`; after `create_project`, publish current `project->to_dict()` stringified JSON to `/api/v1/project/import`.

- [ ] **Step 1: HTTP POST helper** mirroring `create_project` (url `get_runtime_url()+"/api/v1/project/import"`, body = full nested JSON including optional project_id/name).

- [ ] **Step 2: Call sites** — successful create_project callback and/or explicit “Publish to Runtime” menu; debounce on `changed` is optional (prefer explicit + on connect).

- [ ] **Step 3: Manual smoke** — run Go runtime + editor; create S7 device+tag and Modbus device+tag; publish; confirm runtime logs / GET.

- [ ] **Step 4: Commit**

```bash
git commit -m "feat(industrial_editor): publish project via /api/v1/project/import"
```

---

## Spec coverage self-check

| Spec item | Task |
|-----------|------|
| D1 string driver | 3, 7, 10 |
| D2 dual tag schema | 2, 3, 9 |
| D3 catalog data_format | 2, 9 |
| D4 whole import | 5, 10 |
| D5 Normalize scan_groups | 3 |
| D6 no godot names / nested | 3, 5 |
| D7–D8 device EBPro groups + flat+options | 1, 3, 7, 8 |
| Apply hot path | 4, 5 |
| S7 + Modbus TCP | 2, 3, 5, 10 |
| Catalog hygiene | 1 |
| Compat shims | 3, 7 |

## Placeholder scan

No TBD steps; raw address exact strings must be finalized against existing `ParseAddress` in Task 2 (assert via parser, not a guessed literal alone).

---

## Execution handoff

Plan saved to:

- `D:\dt-engine\docs\superpowers\plans\2026-08-28-project-import-tag-schema.md`
- `D:\driver-engine\docs\superpowers\plans\2026-08-28-project-import-tag-schema.md` (copy)

**Two execution options:**

1. **Subagent-Driven (recommended)** — fresh subagent per task, review between tasks  
2. **Inline Execution** — this session with executing-plans and checkpoints  

Which approach?
