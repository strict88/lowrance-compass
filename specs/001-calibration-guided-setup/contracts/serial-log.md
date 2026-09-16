# Contract: Structured Serial Log Lines

Required by constitution Principle IV: every line an HIL script needs to assert on is
machine-parseable. Grammar, fixed across all tags:

```
[TAG] key1=value1 key2=value2 ...
```

- One line per event, `\n`-terminated, ASCII only.
- `TAG` is one of `SYS`, `IMU`, `N2K`, `CAL`, `WIFI`, `DATA` (below).
- Keys never contain `=` or whitespace; string values with spaces are not used — enums/numbers
  only, so a line always splits cleanly on whitespace then `=`.
- Recommended Python parse: `dict(kv.split("=", 1) for kv in line.split()[1:])` after stripping the
  leading `[TAG] `.
- Free-form human text (e.g. a guide "why it failed" explanation) is never put in a structured
  line; it belongs in the UI/`GET /api/guide`, not serial. Structured lines carry a `reason=` enum
  code instead, matching the `error.code` / `calibration_result.reason` vocabulary used in
  `contracts/rest-api.md` and `contracts/websocket.md`.

## `[SYS]` — system/boot

| Example | When |
|---|---|
| `[SYS] boot fw=0.1.0 reset_reason=POWERON heap=234000` | Once, at the end of boot init. |
| `[SYS] heap=123456 uptime=42` | Periodic heartbeat (ties to constitution's `heap=... uptime=...` example). |
| `[DATA] reset record=stage_b reason=crc_mismatch` | A record failed validation and was reset (FR-045/046; see `data-model.md` §5). `reason` ∈ `crc_mismatch`, `schema_mismatch`. |

## `[IMU]` — sensor / heading pipeline

| Example | When |
|---|---|
| `[IMU] hdg=123.4 acc=3 mag=3 accel=3 gyro=3 valid=1` | Every published `HeadingReading` (throttled to a log-friendly rate, not every IMU report). |
| `[IMU] valid=0 reason=SENSOR_ACCURACY_LOW` | Whenever `valid` flips, so a test can catch the transition without diffing every line. `reason` matches `HeadingReading.reason_if_invalid` (`data-model.md` §2.2). |
| `[IMU] disconnected` / `[IMU] reconnected` | Sensor comms loss/recovery. |

## `[N2K]` — NMEA 2000 transmit/receive/bus

| Example | When |
|---|---|
| `[N2K] tx pgn=127250 ok hdg=123.4` | Successful heading PGN transmit (matches constitution's example verbatim). |
| `[N2K] tx pgn=127250 skipped reason=quality_gate` | Transmit suppressed per FR-041 — this, not silence, is how a test confirms the "no heading sent" guarantee (SC-007). |
| `[N2K] bus state=RUNNING\|BUS_OFF\|ERROR_PASSIVE\|BENCH_MODE` | On any bus-state transition. |
| `[N2K] rx pgn=129026 sog=3.1 cog=87.0` | Throttled log of accepted COG/SOG input, used by Stage B/C HIL tests. |

## `[CAL]` — calibration state machine (data-model.md §3)

| Example | When |
|---|---|
| `[CAL] stage=A state=AwaitingPositions positions=3` | On every state-machine transition within a stage (state names match `data-model.md` §3). |
| `[CAL] stage=A result=saved mag=3 accel=3 gyro=3` | Stage A reaches `Done`. |
| `[CAL] stage=A result=timeout` / `result=cancelled` | Stage A ends without saving. |
| `[CAL] stage=C sector_coverage=63 turns=1.4` | Periodic Stage C progress (throttled). |
| `[CAL] stage=C result=rejected reason=fit_residual_too_high rms_deg=5.2` | Stage C rejected; `reason` matches `calibration_result.reason` vocabulary. |
| `[CAL] busy stage=A rejected=B` | A second start was refused per FR-006 (test evidence for the single-session guarantee). |

## `[WIFI]` — settings / network

| Example | When |
|---|---|
| `[WIFI] ssid_changed to=NewName applies_in_s=5` | On an accepted SSID change (FR-036/037). |
| `[WIFI] ap_restart ssid=NewName` | AP actually restarts with the new name. |
| `[WIFI] network_reset trigger=boot_hold` | Recovery path used (FR-038); `trigger` ∈ `boot_hold`, `http_api`. |

## Boot-time and quality-gate HIL assertions this format enables

- **Boot-restore within 10 s (SC-006)**: wait for `[SYS] boot ...` then the first `[IMU] valid=1 ...`
  line, assert the gap is ≤ 10 s.
- **No heading while untrusted (SC-007)**: assert zero `[N2K] tx pgn=127250 ok` lines and at least
  one `[N2K] tx pgn=127250 skipped reason=quality_gate` line while a forced-low-accuracy condition
  holds.
- **Single active session (FR-006)**: assert a `[CAL] busy ...` line appears when a second start is
  attempted during an active procedure.
