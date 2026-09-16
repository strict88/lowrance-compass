# Contract: REST API

Base path: `http://<device-ip>/api`. All bodies are JSON (ArduinoJson v7 on the device side — see
`research.md`). Every response includes `"schema": 1` so clients/tests can detect a breaking
change. Field names/enums here are authoritative for `tasks.md`; types are illustrative JSON types,
not C++ types.

Error shape (any 4xx/5xx):

```json
{ "schema": 1, "error": { "code": "SSID_INVALID", "message": "SSID must be 1-32 characters with no leading or trailing whitespace" } }
```

## GET /api/status

Mirrors everything the UI shows (constitution Principle VI). Polled by tests; also mirrored, at a
higher rate, over the `/ws` WebSocket (`contracts/websocket.md`) for live updates.

```json
{
  "schema": 1,
  "readiness": "NOT_CALIBRATED" | "USABLE_INCOMPLETE" | "READY",
  "heading": {
    "valid": true,
    "heading_deg": 87.3,
    "pitch_deg": -1.2,
    "roll_deg": 0.4,
    "rate_of_turn_deg_s": 0.0,
    "reason_if_invalid": null
  },
  "stages": {
    "a": { "state": "DONE" | "NOT_DONE", "quality": { "mag": 3, "accel": 3, "gyro": 3 }, "saved_at": "2026-09-16T12:00:00Z" | null },
    "b": { "state": "DONE" | "NOT_DONE", "offset_deg": 4.5, "method": "GPS_COURSE" | "KNOWN_BEARING" | null, "saved_at": "..." | null },
    "c": { "state": "DONE" | "NOT_DONE", "max_deviation_deg": 3.1, "residual_rms_deg": 0.6, "source": "GPS_SWING" | "MANUAL_8PT" | null, "saved_at": "..." | null }
  },
  "session": {
    "active_stage": "NONE" | "A" | "B" | "C",
    "sub_state": "string, stage-specific (see data-model.md §3)",
    "progress": { "...": "stage-specific, see below" }
  },
  "n2k": {
    "bus_state": "RUNNING" | "BUS_OFF" | "ERROR_PASSIVE" | "BENCH_MODE",
    "tx_pgn_127250_count": 1234,
    "address": 35,
    "cog_sog_source_present": true,
    "variation_source_present": true
  },
  "settings": { "ssid": "LowranceCompass-1234" },
  "system": { "firmware_version": "0.1.0", "uptime_s": 42, "heap_free": 123456, "reset_reason": "POWERON" }
}
```

`session.progress` shape by `active_stage`:

- **A**: `{ "mag_acc": 0-3, "accel_acc": 0-3, "gyro_acc": 0-3, "positions_done": ["TOP","BOTTOM",...], "rotation_coverage_pct": 0-100 }`
- **B**: `{ "level_set": true, "waiting_reason": "SPEED_TOO_LOW" | "COURSE_NOT_STEADY" | null, "preview_offset_deg": 4.5 | null }`
- **C**: `{ "turns_completed": 1.4, "sector_coverage_pct": 63, "turning_too_fast": false, "manual_point_index": null | 1-8 }`

## GET/POST /api/settings

`GET` returns `{ "schema": 1, "ssid": "..." }`.

`POST` request: `{ "ssid": "NewNetworkName" }`. On success: `{ "schema": 1, "ssid": "NewNetworkName", "applies_in_s": 5 }` — the UI uses `applies_in_s` to show the drop-connection warning (FR-036) before the AP actually restarts. Validation failure (FR-035) → 400 with `error.code = "SSID_INVALID"`, nothing saved.

## POST /api/calibration/{stage}/start

`{stage}` ∈ `a`, `b`, `c`. Body: `{}` for `a`; `{ "mode": "gps" | "manual" }` for `c` (selects §3.3
vs §3.4 in data-model.md); no body for `b` (Stage B has its own sub-endpoints below).

- 200 on success: `{ "schema": 1, "started": true }`.
- 409 if another calibration is already active anywhere (FR-006): `error.code = "CALIBRATION_BUSY"`,
  body also includes `"active_stage"` so the UI can explain what's running.

## POST /api/calibration/{stage}/cancel

`{ "schema": 1, "cancelled": true }`. No-op-safe if nothing was running for that stage.

## POST /api/calibration/{stage}/apply

Valid only for `b` and `c` when `session.sub_state` is a result-ready state (data-model.md §3.2,
§3.3/§3.4); `a` auto-saves on success and has no separate apply step. 200:
`{ "schema": 1, "applied": true, "saved_at": "..." }`. 409 if no candidate result is pending.

## POST /api/calibration/{stage}/discard

Mirrors `apply`; discards the in-memory candidate, keeps whatever was previously saved (or
`NOT_DONE` if nothing was). `{ "schema": 1, "discarded": true }`.

## POST /api/calibration/{stage}/reset

Requires `{ "confirm": true }` in the body (server-side enforcement of the spec's confirmation
step, FR-005 — the UI's own confirmation dialog is in addition to, not instead of, this). Clears
the persisted record for that stage back to `NOT_DONE`. `{ "schema": 1, "reset": true }`.

## POST /api/calibration/b/level

No body. Starts/advances the Stage B level-capture sub-flow (data-model.md §3.2). Response mirrors
`session.progress.b`.

## POST /api/calibration/b/bearing

`{ "bearing_deg": 273.0 }` — a **true** bearing (per the spec's Clarifications); the device
converts to magnetic internally using the same variation source as Stage C. Response includes the
computed `preview_offset_deg` (FR-020) before it is applied via `POST /api/calibration/b/apply`.

## POST /api/calibration/c/manual-point

Used only in `mode: "manual"` (data-model.md §3.4). `{ "reference_heading_deg": 90.0 }` — records
the entered reference for whichever of the 8 points is currently awaited
(`session.progress.c.manual_point_index`). Response advances to the next point or, after the 8th,
moves to `Evaluating`.

## GET /api/guide

Returns the guide content model (FR-008 through FR-011), kept as one content payload so it can be
edited without touching procedure logic:

```json
{
  "schema": 1,
  "stages": {
    "a": {
      "what_and_why": "...",
      "before_you_start": ["..."],
      "steps": [ { "title": "...", "detail": "...", "illustration_svg_id": "stage_a_step1" } ],
      "duration_estimate": "About 3 minutes",
      "success_looks_like": "...",
      "if_it_fails": ["..."]
    },
    "b": { "...": "same shape" },
    "c": { "...": "same shape" }
  }
}
```

`illustration_svg_id` references an inline SVG bundled into the UI's static assets (FR-010) — the
API never serves images itself.

## POST /api/network/reset

Documented recovery path (FR-038) exposed over HTTP too, for a bench/Station-mode scenario where
the physical BOOT-button path (see `quickstart.md`) isn't convenient. Requires
`{ "confirm": true }`. Resets `NetworkSettings` to factory default and restarts the AP; response
mirrors the SSID-change response shape.
