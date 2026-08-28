# Project Import + Dual Tag Schema Design

> Date: 2026-08-28  
> Status: Approved (device UI section revised 2026-08-28)  
> Repos: `D:\dt-engine` (editor) + `D:\driver-engine` (runtime)  
> Related: EBPro device dialog (`FUN_004b94d0`); `docs/specs/2026-08-25-ebpro-device-creation-fields-alignment.md`; `driver_catalog.go`; `tag_field_catalog.go`

## 1. Goal

Unify how the editor pushes industrial project configuration to the Go runtime:

1. **Whole-project import** (not per-entity CRUD as the default path).
2. **String `driver` keys** aligned with backend catalog (`siemens_s7`, `modbus_tcp`, …).
3. **Device fields + UI grouped like EBPro** (common / interface / tuning / protocol-specific); keys match `DeviceDef` + catalog.
4. **Two tag schemas** matching EBPro UI:
   - **absolute** — address mode × address type × data format × offset (EBPro tag dialog).
   - **symbolic** — PLC tag/symbol path (EBPro tag-based / symbolic drivers).
5. **`scan_groups` computed on the backend** via `Project.Normalize()`; clients need not send them.
6. Public API / new code **must not use the word `godot`** in route names or exported identifiers.

### 1.1 Scope (this delivery)

| In scope | Out of scope (later) |
|----------|----------------------|
| `POST /api/v1/project/import` + apply pipeline | Full 235 S7 area-code UI parity |
| Absolute: Siemens S7 Ethernet family keys + `modbus_tcp` | PPI / MPI / Profibus transports |
| Symbolic: schema + UI + import validation; wire any already-registered symbolic drivers | Implementing every symbolic adapter |
| Editor **device** dialog EBPro-style field groups | EBPro binary protocol-code (`0x32`) / `.e30` basename as runtime id |
| Editor **tag** dialog EBPro-style dual layout | Full interlock / AB↔BA conversion columns |
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
| D7 | **Device model + UI align with EBPro** per §3: first-class fields matching `DeviceDef` / catalog; UI grouped like EBPro (not one flat bag of widgets). |
| D8 | Protocol extras (`rack`, `slot`, `tsap_mode`, `slave_id`, …) live under `options` (JSON) / catalog “protocol” group; common/interface/tuning are first-class on the device object. |

## 3. Device model + EBPro UI alignment

Authority: `DeviceDef` + `GET /api/v1/drivers` field lists +  
`docs/specs/2026-08-25-ebpro-device-creation-fields-alignment.md`.

### 3.1 EBPro dialog layout (editor must mirror)

```
┌─ Device properties (EBPro-style) ─────────────────────────────┐
│ §A Common                                                     │
│   Name | HMI/Device (dev_type) | Location | Remote HMI IP     │
│   Device type (4-segment driver dropdown) | Enabled           │
│   (optional read-only hint: default device_id / driver_key)   │
│ §B Interface (driven by driver category_interface)            │
│   Ethernet: IP | Port | Use UDP                               │
│   — or — Serial: COM | Baud | Data/Parity/Stop | Flow | …     │
│ §C Protocol-specific (catalog extras for this driver_key)     │
│   S7: Rack | Slot | TSAP mode | (optional) CPU / max_pdu …    │
│   Modbus TCP: Slave ID | function_code_default | …            │
│ §D Tuning (“Settings” in EBPro — may be sub-panel / fold)     │
│   Poll interval | Timeout | Comm delay | Retries              │
│   Max read/write words | Block size words                     │
└───────────────────────────────────────────────────────────────┘
```

- **§A / §B / §D** keys are shared across drivers (Block A + Eth/Ser + tuning).
- **§C** keys come only from that `driver_key`’s catalog extras.
- Show/hide §B Ethernet vs Serial from `category_interface` / `interface_type`.
- `location_mode=Remote` reveals `remote_hmi_ip` (EBPro behavior).
- Driver dropdown label: Vendor + Series + (Addressing) + (Interface) from catalog metadata (EBPro 4-segment style).

### 3.2 Persisted / import device object (first-class, not only `connection_params`)

Stop treating everything as an opaque `connection_params` blob as the *only* schema. Import and `project.json` use the same shape as backend `DeviceDef` (+ nested `tags` + optional `description`):

```json
{
  "id": "plc_main",
  "name": "plc_main",
  "description": "",
  "driver": "siemens_s7",
  "enabled": true,
  "dev_type": "device",
  "location_mode": "Local",
  "remote_hmi_ip": "",
  "interface_type": "Ethernet",
  "ip": "192.168.0.10",
  "port": 102,
  "use_udp": false,
  "poll_interval": 500,
  "timeout": 1000,
  "comm_delay": 0,
  "retries": 3,
  "max_read_words": 100,
  "max_write_words": 128,
  "block_size_words": 5,
  "options": {
    "rack": 0,
    "slot": 1,
    "tsap_mode": "S7 Basic"
  },
  "tags": [ ]
}
```

Modbus TCP example `options`: `{ "slave_id": 1, "function_code_default": "...", "register_base": "..." }` per catalog.

Rules:

| Rule | Detail |
|------|--------|
| `id` | Defaults to `name` if omitted |
| `driver` | String `driver_key` (D1) |
| Top-level scalars | Match `DeviceDef` JSON tags (`ip`, `port`, `poll_interval`, …) |
| `options` | Protocol-specific catalog keys only |
| Compat | Editor may still *collect* via a Dictionary during UI rebuild, but **serialize** to the flat+`options` shape above |
| Mapping | Import flattens into `DeviceDef` + `BuildConfig()`; extras → `Options` |

### 3.3 Field checklist vs EBPro (must stay in sync)

| Group | Keys (JSON) | EBPro |
|-------|-------------|-------|
| Common | `name`, `dev_type`, `location_mode`, `remote_hmi_ip`, `driver`, `enabled` | Main dialog |
| Ethernet | `interface_type`, `ip`, `port`, `use_udp` | When Ethernet |
| Serial | `serial_port`, `baud_rate`, `data_bits`, `parity`, `stop_bits`, `flow_control`, `station_no`, `broadcast_station_no`, `use_station_variable` | When serial family |
| Tuning | `poll_interval`, `timeout`, `comm_delay`, `retries`, `max_read_words`, `max_write_words`, `block_size_words` | Settings popup |
| S7 options | `rack`, `slot`, `tsap_mode`, (+ catalog: `cpu_type`, `connection_type`, `isoz_eth_mode`, `max_pdu_size` when present) | Protocol |
| Modbus TCP options | `slave_id`, (+ catalog extras) | Protocol |

### 3.4 Catalog hygiene (backend fix-ups in this delivery)

| Issue | Action |
|-------|--------|
| `use_udp` default wrongly typed in catalog | Fix to bool `false` |
| `comm_delay` in `DeviceDef` but missing from some catalog Block A lists | Ensure tuning keys are on every driver field list (or always injected in §D UI from a shared tuning block) |
| `max_pdu_size` used by S7 adapter | Expose on S7 Ethernet catalog extras if missing |
| Legacy editor `connection_params`-only JSON | Accept on import with a one-release compat lift into flat+`options`, then prefer new shape |

### 3.5 Siemens Ethernet keys (runtime-registered)

`siemens_s7`, `siemens_s7_300_ethernet`, `siemens_s7_400_ethernet`, `siemens_s7_200_smart_ethernet` — same ISO-TCP adapter; differ by defaults / extra params.

### 3.6 Modbus TCP

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
2. Implement nested import parser (public name **without** `godot`); accept flat device + `options` (§3.2); deprecate 7-driver `GoDriverName` int map for import.
3. Map catalog `data_format` ids → driver value kinds; map absolute fields → raw address strings for S7/Modbus.
4. `ApplyProject`: after `SetProject` + `Normalize`, sync `device.Manager` / `tags.Registry` / scheduler (fix current “memory only” gap).
5. Import validation against `tag-field-catalog` for tags and `driver_catalog` for device keys.
6. Catalog hygiene §3.4 (`use_udp`, `comm_delay` visibility, S7 `max_pdu_size`).

## 7. Editor changes (`dt-engine`)

1. **Device:** New/Edit device UI regrouped into §A–§D (EBPro); serialize to flat + `options` (§3.2). Evolve `IndustrialDeviceData` beyond a single opaque `connection_params` as the only persisted shape (compat read of old JSON allowed).
2. **Device type dropdown:** 4-segment label from catalog metadata; store string `driver_key` (or index in UI + key on save).
3. **Tag:** `IndustrialTagData` dual-schema fields; dialog switches on `addressing_mode`.
4. `IndustrialRuntimeClient::import_project` posts nested JSON after create_project.
5. Local `res://industrial/project.json` matches import body.
6. Optional menu “Publish to Runtime” + auto-publish on connect when dirty.

## 8. Vocabulary (authoritative)

| Concept | Authority |
|---------|-----------|
| Driver identity | `driver_key` string (catalog) |
| Device common / interface / tuning keys | `DeviceDef` + catalog Blocks A/B/C/D |
| Device protocol extras | catalog extras → JSON `options` |
| Absolute address types / formats | `GET .../tag-field-catalog` |
| Absolute/symbolic mode | catalog `addressing_mode` |
| Scan groups | backend `Normalize()` |

Legacy int enums, `connection_params`-only device JSON, and `int16`/`float` TagDef strings may be accepted in a **compat shim** during migration, then removed.

## 9. Testing

- Unit: absolute→raw address for S7 DBW / Modbus coil & holding; symbolic validation; device flat+`options` → `BuildConfig`.
- Integration: `POST /project/import` with one S7 + one Modbus TCP device (full §3 fields); `apply=true`; tags visible; collect works in mock/live as available.
- Editor smoke: device dialog shows §A–§D correctly for S7 Ethernet and Modbus TCP; tags in both modes; save JSON; import succeeds.

## 10. Non-goals / explicit cuts

- Replicating EBPro interlock / AB↔BA conversion columns in v1 tag schema.
- Shipping all 81 drivers’ adapters in this delivery.
- Using EBPro numeric protocol codes or `.e30` basenames as primary runtime ids.
- Naming routes or packages with `godot`.

## 11. Open follow-ups (post-v1)

- Full S7 address-type table beyond ~23 whitelist.
- Symbolic adapters beyond currently registered keys.
- EBPro gray-line “Device ID, Vx.y, FILE.e30” cosmetic row (optional).
- Remove compat shims and `format=godot` alias.
