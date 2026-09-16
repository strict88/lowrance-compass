# Data Model: Calibration, Guided Setup & Settings

Source of truth: [`spec.md`](./spec.md) (see `## Clarifications` for the decisions baked into the
fields and thresholds below). This document defines persisted records, in-memory state, state
machines, and named thresholds. It does not prescribe language-level types; `research.md` and
implementation tasks choose those.

## 1. Persisted records

All persisted records live in NVS via the `Preferences` API, one namespace per record family, and
each record follows the same envelope so power loss never leaves a half-written record (FR-045,
FR-046; constitution Principle V, VII):

- `schema_version` (uint8): bumped on any field/layout change; a mismatched version on load is
  treated as "fails validation" (see §5).
- `crc32` (uint32): computed over every other field; a mismatch is "fails validation".
- Written using a write-then-verify-then-commit sequence (write to a staging key, read back and
  verify CRC, then atomically swap the "active" pointer key) so a reset mid-write leaves the
  previous valid record in effect, never a torn one.

### 1.1 SensorCalibrationProfile (Stage A)

The BNO08x's Dynamic Calibration Data (DCD) itself persists inside the sensor's own flash via the
SH-2 "save DCD" command (see `research.md` for library support). This record is the ESP32-side
metadata that drives Stage A's status display — it does **not** duplicate the DCD bytes.

| Field | Type | Notes |
|---|---|---|
| `saved_at` | timestamp (§4) | When this profile was accepted and saved (FR-014). |
| `mag_accuracy` | enum 0-3 | SH-2 accuracy at save time; MUST be 3 (High) per FR-014. |
| `accel_accuracy` | enum 0-3 | MUST be 3 (High) per FR-014. |
| `gyro_accuracy` | enum 0-3 | MUST be 3 (High) per FR-014. |
| `firmware_version` | string | Firmware build that performed the save; informational, shown on
  "incompatible after an update" style questions. |

**Persisted status** (FR-002) is derived, not stored separately: presence of a valid record ⇒
`Done`; absent ⇒ `Not done`; cleared by explicit user reset ⇒ `Not done` (`Needs redo` is a UI-only
transient state during an explicit reset confirmation flow, never a persisted value — see §3.1).

### 1.2 InstallationAlignment (Stage B)

| Field | Type | Notes |
|---|---|---|
| `saved_at` | timestamp (§4) | |
| `level_reference_quat` | 4×float32 (w,x,y,z) | Captured attitude at "Set level", stored as a full
  quaternion (not separate pitch/roll offsets) so heading stays correct while tilted. |
| `mounting_offset_rad` | float32, range (−π, π] | Heading correction from FR-018/FR-019. |
| `offset_method` | enum: `KNOWN_BEARING` \| `GPS_COURSE` | Which method produced the offset,
  shown in the UI per FR-021. |

### 1.3 DeviationCorrection (Stage C)

| Field | Type | Notes |
|---|---|---|
| `saved_at` | timestamp (§4) | |
| `coefficients` | 5×float32: `A, B, C, D, E` | `deviation(θ) = A + B·sinθ + C·cosθ + D·sin2θ + E·cos2θ`. |
| `max_deviation_rad` | float32 | Max \|deviation\| found across the fitted curve (FR-027). |
| `residual_rms_rad` | float32 | RMS fit residual, shown to the user before Apply (FR-027). |
| `source` | enum: `GPS_SWING` \| `MANUAL_8PT` | Which procedure produced this result. |

### 1.4 NetworkSettings

| Field | Type | Notes |
|---|---|---|
| `ssid` | string, 1-32 bytes, no leading/trailing whitespace | FR-035. |

Wi-Fi password is out of scope for this feature (see spec Assumptions) and is not part of this
record; any existing password field/default is left untouched by this feature's code paths.

### 1.5 Threshold/config values

Not persisted — compiled-in named constants (tunable without touching calibration/procedure logic,
consistent with FR-011). Exact numeric defaults are chosen and justified in `research.md` /
implementation, not fixed here; this table fixes the **name**, **unit**, and **purpose** of every
threshold referenced anywhere in this document so tasks.md can implement each as a single named
symbol.

| Constant | Unit | Purpose |
|---|---|---|
| `STAGE_A_STILLNESS_GYRO_VAR_MAX` | (rad/s)² | Gyro variance ceiling to count as "still". |
| `STAGE_A_STILLNESS_HOLD_S` | s | How long stillness must hold for the gyro step. |
| `STAGE_A_POSITION_HOLD_S` | s | How long each of the six rest positions must hold (~3 s per spec). |
| `STAGE_A_POSITION_GRAVITY_TOL_DEG` | deg | Angular tolerance for "gravity is near this axis". |
| `STAGE_A_ROTATION_COVERAGE_BINS` | count | Number of sphere-surface bins tracked for rotation coverage. |
| `STAGE_A_ROTATION_COVERAGE_MIN_FRACTION` | 0-1 | Fraction of bins that must be visited. |
| `STAGE_A_REQUIRED_ACCURACY` | enum | Fixed at High (3) for all three sensors, per FR-014 — not tunable. |
| `STAGE_A_TIMEOUT_S` | s | Overall Stage A timeout (FR-015). |
| `HEADING_MIN_ACCURACY` | enum | Live transmit-gate threshold; fixed at High (3) for all three sensors, same as `STAGE_A_REQUIRED_ACCURACY` per the spec's Clarifications. |
| `STAGE_B_LEVEL_STILL_WINDOW_S` | s | Averaging window for "Set level". |
| `STAGE_B_LEVEL_GYRO_VAR_MAX` | (rad/s)² | Stillness ceiling reused for the level capture. |
| `STAGE_B_GPS_MIN_SOG` | m/s | Minimum speed to accept GPS-course samples (FR-019). |
| `STAGE_B_GPS_COG_STEADY_WINDOW_S` | s | Window over which course steadiness is judged. |
| `STAGE_B_GPS_COG_STEADY_MAX_STDDEV_DEG` | deg | Max course standard deviation to count as "steady". |
| `STAGE_C_MIN_SOG` | m/s | Minimum speed for an accepted swing sample (FR-025). |
| `STAGE_C_MAX_TURN_RATE_DEG_S` | deg/s | Turn-rate ceiling for an accepted sample / "too fast" warning (FR-023). |
| `STAGE_C_TURN_RATE_STEADY_TOL_DEG_S` | deg/s | How much turn-rate may vary and still count as "even". |
| `STAGE_C_SECTOR_COUNT` | count | 36 (10° sectors) per the plan's model. |
| `STAGE_C_MIN_SAMPLES_PER_SECTOR` | count | Minimum accepted samples before a sector counts as covered. |
| `STAGE_C_MIN_FULL_TURNS` | count | 2, per FR-022/spec. |
| `STAGE_C_MAX_RMS_RESIDUAL_DEG` | deg | Fit-quality acceptance ceiling (FR-026). |
| `STAGE_C_MAX_ABS_DEVIATION_DEG` | deg | Sanity ceiling on any single fitted deviation value (FR-026). |
| `STAGE_C_OUTLIER_MAD_MULTIPLIER` | unitless | Median-absolute-deviation multiplier for per-sector outlier rejection. |
| `STAGE_C_REFERENCE_MAX_AGE_S` | s | Max age of the last COG/SOG/variation bus message to count as "fresh". |
| `STAGE_C_MANUAL_POINT_COUNT` | count | 8, fixed (N/NE/E/SE/S/SW/W/NW). |
| `SSID_MIN_LEN` / `SSID_MAX_LEN` | bytes | 1 / 32, fixed by FR-035. |
| `CALIBRATION_SESSION_INACTIVITY_TIMEOUT_S` | s | Auto-cancel/time-out an in-progress procedure with no client activity (FR-007). |
| `NETWORK_RESET_BOOT_HOLD_S` | s | How long the BOOT button must be held to trigger the network recovery path (FR-038; see `research.md`/`quickstart.md` for the chosen mechanism). |
| `WS_STATUS_RATE_HZ_IDLE` | Hz | WebSocket status broadcast rate outside an active calibration. |
| `WS_STATUS_RATE_HZ_ACTIVE_CAL` | Hz | WebSocket broadcast rate while a calibration is in progress (bounded so it never starves `ImuTask`/`N2kTask`, per constitution Principle III). |

## 2. In-memory / live state (not persisted)

### 2.1 CalibrationSession

The single active procedure, enforced unique across all clients (FR-006).

| Field | Notes |
|---|---|
| `active_stage` | `NONE \| A \| B \| C` |
| `sub_state` | Stage-specific (§3). |
| `started_at` | Monotonic, for timeout calculation. |
| `last_client_activity_at` | Monotonic; drives `CALIBRATION_SESSION_INACTIVITY_TIMEOUT_S`. |
| `progress` | Stage-specific progress payload (e.g. Stage A per-sensor accuracy + position bitmap;
  Stage C sector-coverage bitmap + turn count). |
| `candidate_result` | Stage B/C only: the computed-but-not-yet-applied offset/deviation, held in
  memory until Apply/Discard (FR-021, FR-028). Never touches persisted storage until accepted. |

`active_stage == A` also drives the heading pipeline's quality-gate freeze — see §6's "Stage A
in-progress freeze" for how an in-progress Stage A attempt keeps heading transmitting from the
previously saved `SensorCalibrationProfile` instead of live accuracy (FR-041).

### 2.2 HeadingReading (published snapshot)

The single value both the UI and NMEA 2000 output read from (FR-042), updated by the heading
pipeline (§6) on every IMU report.

| Field | Notes |
|---|---|
| `heading_rad` | `[0, 2π)`, magnetic reference. |
| `pitch_rad`, `roll_rad` | Post level-reference. |
| `rate_of_turn_rad_s` | |
| `valid` | Whether this snapshot currently passes `HEADING_MIN_ACCURACY` (FR-041). |
| `reason_if_invalid` | e.g. `SENSOR_NOT_CALIBRATED`, `SENSOR_ACCURACY_LOW`, `SENSOR_DISCONNECTED` —
  surfaced verbatim in the UI (FR-041) and in `[N2K]`/`[IMU]` serial log lines. |

`valid`/`reason_if_invalid` are normally derived from live per-sensor accuracy (§6's quality
gate), except while a Stage A attempt is in progress and a previously saved calibration exists —
see §6's "Stage A in-progress freeze" (FR-041).

### 2.3 StageStatus (×3, one per stage)

Drives FR-001/FR-002. Distinct from `CalibrationSession`, which only exists while a procedure is
actively running.

| Field | Notes |
|---|---|
| `persisted_state` | `NOT_DONE \| DONE` — see §3.1 for why there is no automatic `NEEDS_REDO`. |
| `quality` | Present only when `DONE`; stage-specific quality summary (Stage A: 3 accuracy levels;
  Stage B: n/a beyond "done"; Stage C: residual/max-deviation). |
| `saved_at` | Present only when `DONE`. |

## 3. State machines

Timeout/cancel transitions always return to the state the system was in before the attempt started
(`Idle`, or `Done` with the previous record untouched) — never to a half-applied state.

### 3.1 Stage A

```
Idle ──Start──▶ AwaitingStillness ──gyro var below STAGE_A_STILLNESS_GYRO_VAR_MAX
                                     for STAGE_A_STILLNESS_HOLD_S──▶ AwaitingPositions
AwaitingPositions ──all 6 positions held STAGE_A_POSITION_HOLD_S
                                     each, gravity within tol──▶ AwaitingRotation
AwaitingRotation ──rotation-coverage fraction ≥ STAGE_A_ROTATION_COVERAGE_MIN_FRACTION──▶ Evaluating
Evaluating ──mag/accel/gyro accuracy all == STAGE_A_REQUIRED_ACCURACY──▶ SaveDCD ──▶ Done
Evaluating ──otherwise, and STAGE_A_TIMEOUT_S elapsed──▶ TimedOut ──▶ Idle (previous record kept)
(any state) ──Cancel──▶ Idle (previous record kept)
```

Per the spec's Clarifications, no state here ever automatically marks the persisted `StageStatus`
as needing redo — only an explicit user Reset (a separate, UI-only action, FR-005) changes
`persisted_state` from `DONE` back to `NOT_DONE`. A live accuracy drop during normal operation
(outside this state machine) affects `HeadingReading.valid` only (§2.2, §6), never `StageStatus`.

### 3.2 Stage B

```
Idle ──"Set level" pressed──▶ LevelCapturing (avg over STAGE_B_LEVEL_STILL_WINDOW_S,
                                              gyro var < STAGE_B_LEVEL_GYRO_VAR_MAX)
LevelCapturing ──window complete──▶ LevelSet (level_reference_quat computed, held as candidate)
LevelSet ──user picks Known bearing──▶ AwaitingBearingEntry
LevelSet ──user picks GPS course, COG source detected──▶ AwaitingGpsAlignment
AwaitingBearingEntry ──bearing entered──▶ OffsetComputed
AwaitingGpsAlignment ──SOG ≥ STAGE_B_GPS_MIN_SOG and COG stddev ≤
                        STAGE_B_GPS_COG_STEADY_MAX_STDDEV_DEG over
                        STAGE_B_GPS_COG_STEADY_WINDOW_S──▶ OffsetComputed
                     (otherwise stays here, surfacing why it's waiting, per FR-019)
OffsetComputed ──Accept──▶ Saved ──▶ Idle (record persisted)
OffsetComputed ──Discard──▶ Idle (no change)
```

### 3.3 Stage C — GPS swing

```
Idle ──Start swing──▶ Swinging (recording samples; sector-coverage bitmap fills as
                                 STAGE_C_MIN_SAMPLES_PER_SECTOR accepted samples land in each
                                 of STAGE_C_SECTOR_COUNT sectors, across ≥ STAGE_C_MIN_FULL_TURNS)
Swinging ──user stops / full coverage reached──▶ Evaluating
Evaluating ──fit RMS residual ≤ STAGE_C_MAX_RMS_RESIDUAL_DEG and max|deviation| ≤
              STAGE_C_MAX_ABS_DEVIATION_DEG and full sector coverage──▶ ResultReady
Evaluating ──otherwise──▶ Rejected ──▶ Idle (previous correction, if any, kept; reason shown)
ResultReady ──Apply──▶ Saved ──▶ Idle (record persisted, new correction now live)
ResultReady ──Discard──▶ Idle (previous correction, if any, kept)
(any state) ──Cancel──▶ Idle (previous correction kept)
```

Heading transmission uses the previous (or absent) `DeviationCorrection` throughout `Swinging` /
`Evaluating` / `ResultReady`; only entering `Saved` swaps the live correction (FR-029).

### 3.4 Stage C — manual 8-point swing (fallback, FR-030)

Same `Evaluating`/`ResultReady`/`Saved` tail as §3.3, but the entry differs:

```
Idle ──Start manual swing──▶ AwaitingPoint(1..8) ──user stops boat on compass point N,
                                                      confirms, enters reference value──▶
                              AwaitingPoint(N+1) … AwaitingPoint(8) ──▶ Evaluating
```

## 4. Timestamps ("date" shown in stage status)

The device has no internet/NTP. Wall-clock dates for `saved_at` fields are sourced from NMEA 2000
System Time (PGN 126992), when the bus has provided it at least once since boot. If no bus time has
ever been received, the record still saves (never blocked on this), but the UI shows "date
unavailable" instead of a calendar date until a bus time source appears. This is a planning-level
default, not a spec clarification, because it doesn't change any user-facing functional behavior
described in the spec — only how a "date" is sourced.

## 5. Validation & recovery (FR-045, FR-046)

On boot, and before use, each record is loaded and checked: `schema_version` matches the running
firmware's expected version for that record type, and `crc32` matches the computed checksum over
the rest of the record. Either check failing means "fails validation" per FR-045: that specific
record resets to its default (`NOT_DONE` / absent), every other record is left untouched, and a
structured serial log line plus a status-data field records which record was reset and why
(`schema_mismatch` vs `crc_mismatch`) — see `contracts/serial-log.md`.

## 6. Heading pipeline (fixed order, constitution Principle I / FR-044)

```
raw rotation vector + per-sensor accuracy (from ImuDriver)
  → apply level_reference_quat (Stage B, identity if Stage B not done)
  → apply mounting_offset_rad (Stage B, 0 if Stage B not done)
  → apply DeviationCorrection curve (Stage C, 0 if Stage C not done)
  → normalize to [0, 2π)
  → quality gate: valid = (mag_accuracy == accel_accuracy == gyro_accuracy == HEADING_MIN_ACCURACY)
  → HeadingReading snapshot (§2.2), published to both WebApi and N2kService
```

Stage B/C corrections are applied additively/independently of whether the *other* stage is done —
an uncalibrated Stage B or C contributes an identity/zero correction rather than blocking the
pipeline, so partial setups (per spec User Story 2) still produce a heading whenever Stage A alone
passes the quality gate.

**Stage A in-progress freeze (FR-041):** while `CalibrationSession.active_stage == A` (§2.1, a
Stage A attempt is currently running) **and** a previously saved `SensorCalibrationProfile` (§1.1)
exists, the quality gate step's mag/accel/gyro accuracy inputs are taken from that persisted
profile — which was all-`HEADING_MIN_ACCURACY` at save time by definition — instead of the live
`ImuDriver` report. This is what "the device MUST continue transmitting heading using the
previously saved calibration (not the in-progress attempt)" (FR-041) means in pipeline terms: the
rotation vector itself (`heading_rad`/`pitch_rad`/`roll_rad`/`rate_of_turn_rad_s`) still comes from
the live IMU report as normal — only the accuracy *gate* is held at the persisted profile's value,
so ordinary in-progress recalibration (redoing the six positions/rotation, per User Story 2's
"Recalibrate") never trips `HeadingReading.valid` to false just because live accuracy is
transiently below `HEADING_MIN_ACCURACY` mid-attempt. If no previously saved profile exists (the
first-ever Stage A attempt on a fresh device), there is nothing to freeze to and the gate falls
back to live accuracy as described above, which will generally stay invalid throughout the
attempt — consistent with FR-041's own precondition ("a previously saved, valid Stage A
calibration exists"). This freeze applies to the quality gate only; it does not apply to Stage B
or Stage C attempts, whose own "use the previous value" behavior is already covered by §3.2/§3.3
substituting the previous `InstallationAlignment`/`DeviationCorrection` record for the in-progress
candidate.
