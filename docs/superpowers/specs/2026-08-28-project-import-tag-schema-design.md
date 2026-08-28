# Project Import + Dual Tag Schema Design

> Date: 2026-08-28  
> Status: Approved (pending user review of this written spec)  
> Repos: `D:\dt-engine` (editor) + `D:\driver-engine` (runtime)  
> Related: EBPro reverse-engineering tag dialog; `internal/api/driver_catalog.go`; `internal/api/tag_field_catalog.go`

## 1. Goal

Unify how the editor pushes industrial project configuration to the Go runtime:

1. **Whole-project import** (not per-entity CRUD as the default path).
2. **String `driver` keys** aligned with backend catalog (`siemens_s7`, `modbus_tcp`, …).
3. **Two tag schemas** matching EBPro UI:
   - **absolute** — address mode × address type × data format × offset (EBPro tag dialog).
   - **symbolic** — PLC tag/symbol path (EBPro tag-based / symbolic drivers).
4. **`scan_groups` computed on the backend** via `Project.Normalize()`; clients need not send them.
5. Public API / new code **must not use the word `godot`** in route names or exported identifiers.

### 1.1 Scope (this delivery)

| In scope | Out of scope (later) |
|----------|----------------------|
| `POST /api/v1/project/import` + apply pipeline | Full 235 S7 area-code UI parity |
| Absolute: Siemens S7 Ethernet family keys + `modbus_tcp` | PPI / MPI / Profibus transports |
| Symbolic: schema + UI + import validation; wire any already-registered symbolic drivers | Implementing every symbolic adapter |
| Editor tag dialog EBPro-style dual layout | EBPro binary protocol-code (`0x32`) file format |
| Local `project.json` same shape as import body | Per-device/tag incremental sync as default |

## 2. Decisions (locked)

| # | Decision |
|---|----------|
| D1 | `driver` is a **string key** from `GET /api/v1/drivers` / catalog `driver_key`. |
| D2 | Tag persistence follows EBPro **two schemas** (absolute vs symbolic), not a single flattened `address`+legacy `data_type` as the only truth. |
| D3 | Absolute `data_format` IDs come from **tag-field-catalog** (`i16`, `f32`, `bit`, …), not the old TagDef vocabulary (`int16`, `float`) as the public contract. |
| D4 | Default sync path is **one import** of nested project JSON; device/tag CRUD APIs remain for ops, not editor default. |
| D5 | `scan_groups` optional on import; backend `Normalize()` fills from `poll_interval`. |
| D6 | Naming: `POST /api/v1/project/import`; export query `format=nested` (prefer over legacy `format=godot`). |

## 3. Device payload (import)

```json
{
  "name": "plc_main",
  "driver": "siemens_s7",
  "enabled": true,
  "description": "",
  "connection_params": {
    "ip": "192.168.0.10",
    "port": 102,
    "rack": 0,
    "slot": 1,
    "poll_interval": 500,
    "timeout": 1000,
    "tsap_mode": "S7 Basic"
  },
  "tags": [ ]
}
```

- Field keys in `connection_params` = catalog field `key` values (backend-authoritative).
- Common scalars (`ip`, `port`, `poll_interval`, …) map into `DeviceDef` top-level via existing `BuildConfig()` patterns; protocol extras remain in `Options`.
- Device `id` defaults to `name` when omitted (editor today has no separate id).

### 3.1 Siemens Ethernet keys (runtime-registered)

`siemens_s7`, `siemens_s7_300_ethernet`, `siemens_s7_400_ethernet`, `siemens_s7_200_smart_ethernet` — same ISO-TCP adapter; differ by defaults / extra params.

### 3.2 Modbus TCP

`modbus_tcp` (and other Modbus-family keys only when the same factory already registers them).

## 4. Tag schemas

Discriminator: optional `"schema": "absolute" | "symbolic"`.  
If omitted, infer from the parent device’s driver → `tag-field-catalog.addressing_mode`.

### 4.1 Absolute (EBPro three-column dialog)

**UI (editor):**

```
Name | Description | Device
Address mode [Bit|Word] | Address type […] | Data format […] | Address [offset]
(conditional) DB number | Length
Writable | Poll interval (ms, optional)
```

**Persisted fields:**

| Field | Type | Required | Notes |
|-------|------|----------|-------|
| `name` | string | yes | |
| `description` | string | no | |
| `address_mode` | `"bit"` \| `"word"` | yes | EBPro bit/word; not absolute/symbolic |
| `address_type` | string | yes | Catalog address type `id` |
| `data_format` | string | word: yes; bit: `"bit"` | Catalog format `id`; Bool is bit-mode, not a format dropdown item |
| `address` | string | yes | **Offset / numeric part only** |
| `db_number` | int | if type `requires_db` | S7 DB family |
| `length` | int | if type `has_length` | STRING family |
| `writable` | bool | no (default false) | |
| `poll_interval` | int | no | Feeds Normalize |
| `scale` | object | no | `{enabled, raw_min, raw_max, eng_min, eng_max}` |

**Example:**

```json
{
  "schema": "absolute",
  "name": "Speed",
  "address_mode": "word",
  "address_type": "DBn",
  "db_number": 1,
  "address": "0",
  "data_format": "i16",
  "writable": false,
  "poll_interval": 500
}
```

**Runtime derivation:** backend builds the driver raw address string (e.g. `DB1.DBW0`, `4x_HR 100`, `0x Coil 5`) and internal value kind from `(address_mode, address_type, data_format, address, db_number, length)` using catalog metadata + existing `ParseAddress` helpers.

### 4.2 Symbolic (EBPro tag-based)

**UI (editor):**

```
Name | Description | Device
PLC tag / symbol [________________]     ← primary; no address-type table
Data format [optional]
[Import from project…] when catalog.symbolic_import.supported
Writable | Poll interval (ms, optional)
```

**Persisted fields:**

| Field | Type | Required | Notes |
|-------|------|----------|-------|
| `name` | string | yes | |
| `description` | string | no | |
| `symbol` | string | yes | PLC tag path / symbol (EBPro “use tag name”) |
| `data_format` | string | no | Catalog id when applicable |
| `writable` | bool | no | |
| `poll_interval` | int | no | |
| `scale` | object | no | |

**Example:**

```json
{
  "schema": "symbolic",
  "name": "TankTemp",
  "symbol": "Program:Main.Tank.Temperature",
  "data_format": "f32",
  "writable": false
}
```

**Validation:** absolute device + symbolic tag (or reverse) → HTTP 400 with clear error. Symbolic driver without adapter → 400/501 stating unsupported, not silent success.

## 5. HTTP API

### 5.1 `POST /api/v1/project/import`

Request body:

```json
{
  "project_id": "optional",
  "project_name": "optional",
  "apply": true,
  "devices": [ /* nested devices + tags */ ]
}
```

- Do **not** require `scan_groups`.
- `apply` default `true`: replace in-memory project, re-register devices/tags, rebuild scheduler (hot apply). `false`: store project only (debug).
- Response: `{ "status": "imported", "devices": N, "tags": M, "scan_groups": K, "applied": true }`.

### 5.2 Export

- Prefer `GET /api/v1/project?format=nested` returning the nested editor-compatible JSON (dual tag schemas).
- Legacy `format=godot` may remain temporarily as an alias; new code uses `nested`.

### 5.3 Existing endpoints

- `POST /api/v1/project` — create/register `{project_id, project_name}` only (unchanged).
- Device/tag CRUD — remain for ops; editor default is import.

## 6. Backend changes (`driver-engine`)

1. Extend `TagDef` (or parallel import DTO → TagDef) with absolute/symbolic fields; keep a derived raw `Address` + value-kind for collectors/drivers.
2. Implement nested import parser (public name **without** `godot`); deprecate reliance on 7-driver `GoDriverName` int map for import.
3. Map catalog `data_format` ids → driver value kinds; map absolute fields → raw address strings for S7/Modbus.
4. `ApplyProject`: after `SetProject` + `Normalize`, sync `device.Manager` / `tags.Registry` / scheduler (fix current “memory only” gap).
5. Import validation against `tag-field-catalog` rules for the device’s `driver_key`.

## 7. Editor changes (`dt-engine`)

1. `IndustrialTagData` stores dual-schema fields (not only int `data_type` + single address string as sole truth).
2. New/Edit Tag dialog: layout switches on device driver’s `addressing_mode` (EBPro absolute vs symbolic).
3. `IndustrialRuntimeClient::import_project` posts nested JSON after create_project; convert UI driver index → string key via `industrial_get_driver_key` / runtime catalog.
4. Local `res://industrial/project.json` uses the same nested shape as import.
5. Optional menu “Publish to Runtime” + auto-publish on connect when dirty.

## 8. Vocabulary (authoritative)

| Concept | Authority |
|---------|-----------|
| Driver identity | `driver_key` string (catalog) |
| Absolute address types / formats | `GET .../tag-field-catalog` |
| Absolute/symbolic mode | catalog `addressing_mode` |
| Scan groups | backend `Normalize()` |
| Connection params | catalog field `key` list |

Legacy int enums / `int16`/`float` TagDef strings may be accepted in a **compat shim** during migration, then removed.

## 9. Testing

- Unit: absolute→raw address for S7 DBW / Modbus coil & holding; symbolic validation branches.
- Integration: `POST /project/import` with one S7 + one Modbus TCP device; `apply=true`; `GET /tags` or diagnostics show tags; collect succeeds in mock/live as available.
- Editor smoke: create tags in both UI modes, save JSON, import succeeds.

## 10. Non-goals / explicit cuts

- Replicating EBPro interlock / AB↔BA conversion columns in v1 tag schema.
- Shipping all 81 drivers’ adapters in this delivery.
- Naming routes or packages with `godot`.

## 11. Open follow-ups (post-v1)

- Full S7 address-type table beyond ~23 whitelist.
- Symbolic adapters beyond currently registered keys.
- Remove compat shims and `format=godot` alias.
