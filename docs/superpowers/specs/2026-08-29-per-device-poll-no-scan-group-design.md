# Per-device poll — remove scan_group (EBPro-aligned)

Date: 2026-08-29  
Repos: `driver-engine` + `dt-engine` (branch `feat/project-import-tag-schema`)  
Related: EBPro device period at `device+0xC74`; prior scan-group model in PRD §9.1

## 1. Goal and invariants

Align runtime collection with EBPro: **one poll interval per device; all tags on that device are collected together on that interval.** Completely remove the `scan_group` / `scan_groups` concept from config, API, scheduler, CSV, and editor.

Invariants:

1. Scheduler clock source = `DeviceDef.poll_interval` (default 1000 ms; still subject to `MinScanIntervalMs`).
2. One tick = `CollectDevice(ctx, deviceID)`: read all tags for that device (existing MergeKey / MaxBatch splitting stays).
3. **`scan_group` and `scan_groups` must not appear** in any project JSON, import/export payload, API request/response, CSV header/row, or Godot serialize path. Presence of these keys → **hard validation failure** (explicit error naming the field). No silent strip, no ignore.
4. Device `enabled=false` or device with zero tags → no ticker.

## 2. Data model / API / scheduler / editor

### 2.1 driver-engine data model

Remove:

- `Project.ScanGroups`, type `ScanGroupDef`
- `TagDef.ScanGroup`
- Nested / Godot / CSV fields and columns named `scan_group` / `scan_groups` / `ScanGroup`
- Tag-level `poll_interval` if present (avoid dual clock); device `poll_interval` is sole clock

Keep: `DeviceDef.poll_interval` (and device `enabled`).

`Normalize()`: no longer creates or fills scan groups; only other defaults (endpoint synthesis, enabled, etc.).

`Validate()` (and import parsers): if root, any device, or any tag JSON object contains key `scan_groups` or `scan_group` → error.

### 2.2 Scheduler / collector

- Scheduler: one goroutine + `time.Ticker` per enabled device that has ≥1 tag; interval = that device’s `poll_interval`.
- Collector: replace `CollectGroup` with `CollectDevice`; remove `NamesByGroup` / group-indexed registry APIs used only for scanning.
- `Start` / `Reload` / ApplyProject: rebuild tickers from the device list.

### 2.3 HTTP API

- Tag create/update: drop `scan_group` field (no longer required).
- `POST /api/v1/project/import` and project GET/export: body and response must not include `scan_groups` / `scan_group`; if present → **400** with clear message.
- Import response stats: `devices` / `tags` only (no `scan_groups` count).

### 2.4 dt-engine editor (`industrial_editor`)

- Device form: remove Scan Group row and related load/save.
- `IndustrialProject` persist / publish / CSV: never emit `scan_group` / `scan_groups`.
- Device dock “view by scan group” (if present): remove or replace with device/enabled-only grouping.
- New-device dialog already omits Scan Group; after this change, create vs property field sets no longer differ by Scan Group.

## 3. Migration / risks

### Migration

- No auto-strip of old keys: callers must remove `scan_group` / `scan_groups` before import.
- Update `examples/project.json`, Postman collection, all fixtures and docs in-repo so no remaining *valid* sample contains those keys.
- Godot projects saved with old schema must be re-saved/published with the new editor; old publish payloads are rejected by runtime.

### Risks

| Risk | Handling |
|------|----------|
| Devices that shared a named group at different rates | Each device now follows its own `poll_interval` (EBPro behavior); document the change |
| External CSV scripts expecting ScanGroup column | Update headers + `csv_interop` tests; note in changelog |
| Empty “by scan group” UI | Delete or replace in same change set |
| Tests asserting `group_Xdms` from Normalize | Retarget to per-device collect |

## 4. Acceptance — full device & tag management retest

Acceptance is **not** limited to scheduler unit tests. After implementation, **retest the full device-management and tag-management stack end-to-end** (backend + Godot editor).

### 4.1 Backend (driver-engine)

- [ ] `go test ./...` green, including rewritten scheduler/collector tests (per-device tick).
- [ ] Import / load with `scan_groups` or any `scan_group` → fail; clean project → succeed.
- [ ] Device CRUD API (create/update/delete/list, enable/disable, connection fields, `poll_interval`).
- [ ] Tag CRUD API (create/update/delete/list under device; absolute + symbolic schemas as applicable).
- [ ] `POST /project` + `POST /project/import` + apply/reload: devices and tags live-collect at each device’s interval.
- [ ] CSV import/export round-trip without ScanGroup columns.
- [ ] Integration suites that touch devices/tags/project (crud, project_import, project_compat, csv_interop, modbus smoke as environment allows).

### 4.2 Editor (dt-engine industrial_editor)

- [ ] New device: create with connection/tuning fields; no Scan Group UI; persists and publishes without forbidden keys.
- [ ] Device properties dock: edit name/desc/driver/enabled/params/`poll_interval`; no Scan Group; save + publish.
- [ ] Device dock: list/select/delete/search; grouping modes without scan group.
- [ ] New tag / edit tag / batch generate: bind to device; publish; runtime sees tags on that device’s poll.
- [ ] Tag dock / device-embedded tag list: filter, select, delete as before.
- [ ] Create vs property: field sets consistent (no extra Scan Group on properties).
- [ ] Reject path: opening/publishing a legacy file that still contains `scan_group` surfaces a clear error (editor and/or runtime).

### 4.3 Cross-process smoke

- [ ] Runtime starts empty → editor register project → import devices+tags → values update on device poll cadence.
- [ ] Change one device’s `poll_interval`, re-import/apply → that device’s collect rate changes; others unchanged.
- [ ] Disable device → its tags stop collecting / show bad or stale per existing quality rules; re-enable recovers.

## 5. Non-goals

- Reintroducing named multi-device scan groups.
- Silent compatibility shims that accept and drop `scan_group` keys.
- Changing EBPro Polling Block / OPC per-tag scan-rate semantics in foreign binaries (reference only).

## 6. Implementation order (for plan)

1. Backend model + Validate reject keys + Normalize slim + scheduler/collector per-device.
2. API + examples + unit/integration tests.
3. Editor remove Scan Group + serialize cleanup.
4. Full §4 acceptance matrix (device + tag, FE + BE).
