# Quickstart: Calibration, Guided Setup & Settings

Validation guide for this feature, per constitution Principle IV (Hardware-in-the-Loop
Verification). Exact commands assume PlatformIO CLI (`pio`) on PATH and the device connected over
USB with IMU and CAN transceiver wired (see **Wiring** below). Library/platform versions referenced
here are pinned in `platformio.ini`; see `research.md` for the exact values and rationale.

## Wiring (this feature's hardware, per the pin configuration header)

| Signal | GPIO | Notes |
|---|---|---|
| I2C SDA (BNO08x) | 8 | |
| I2C SCL (BNO08x) | 9 | |
| BNO08x INT | 15 | Drives the interrupt-driven `ImuTask` design. |
| BNO08x RST | 16 | Used for hard sensor reset after a comms fault. |
| TWAI TX (to VP230 TXD) | 4 | Default proposed pin, not yet physically wired at plan time — confirm/rewire if different. |
| TWAI RX (from VP230 RXD) | 5 | Same as above. |

All six avoid the constitution's reserved ranges (flash/PSRAM 26–37, native USB 19/20, strapping
0/3/45/46). These live in one pin configuration header (constitution Principle V) generated in
Phase 0 of `tasks.md` — this table is the source values for that header, not a substitute for it.

## Environments

- `esp32s3` — the real firmware, flashed to the device.
- `esp32s3_test` — on-target Unity tests (IMU comms, TWAI start/stop/bus-off, NVS record
  read/corrupt/recover, LittleFS asset presence).
- `native` — host-side Unity tests for every pure-logic module (`heading/`, `calibration/`,
  `n2k_codec/`, `settings/`).

## 1. Native tests (no hardware required)

```
pio test -e native
```

Expected: all suites pass, including — angle wraparound at 0°/360°, heading-pipeline stage
ordering (`data-model.md` §6), Stage A step-detection on recorded/synthetic IMU traces, Stage B
offset math (including the true→magnetic bearing conversion), Stage C curve fit on synthetic data
with known coefficients/noise/missing-sectors/outliers (SC-009), SSID validation, record
CRC/schema versioning, and PGN field encode/decode.

## 2. Build and flash the firmware

```
pio run -e esp32s3
pio run -e esp32s3 -t upload
```

Release the serial port before upload if a monitor is attached (constitution Principle IV).

## 3. On-target tests

```
pio test -e esp32s3_test
```

## 4. Serial HIL scripts

```
python tools/hil/capture.py --port <PORT> --timeout 15 --expect "[SYS] boot"
python tools/hil/boot_restore_check.py --port <PORT> --timeout 10   # SC-006
python tools/hil/quality_gate_check.py --port <PORT> --timeout 30   # SC-007
python tools/hil/stage_a_walkthrough.py --port <PORT>               # prompts you to move the device
```

Every script opens the serial port with a hard timeout, asserts on the structured lines in
`contracts/serial-log.md`, and always closes/releases the port on exit (including on failure) so a
subsequent `upload` never fights over the port.

## 5. Bench mode (no other NMEA 2000 node present)

Enable TWAI self-test/no-ACK mode so the device can transmit without another node acknowledging:

```
pio run -e esp32s3 -t upload --upload-port <PORT>   # build with -D BENCH_MODE=1, or
curl -X POST http://<device-ip>/api/... (runtime toggle, if implemented as a setting)
```

`GET /api/status` → `n2k.bus_state == "BENCH_MODE"` confirms it's active; `[N2K] bus
state=BENCH_MODE` appears in the serial log. Bench mode is clearly flagged in the UI too — never
silently indistinguishable from a real bus connection. Real-bus verification (frames actually
acknowledged by the Elite FS 9) remains a manual checklist item (`checklists/manual-verification.md`,
to be created in Phase 1 of `tasks.md`) until performed on the actual boat/bench-with-second-node.

## 6. Simulated GPS for Stage B / Stage C (debug builds only)

Stage B's GPS-course method and Stage C's swing both need COG/SOG/variation on the bus. On the
bench, without the Elite FS 9 actually moving, inject synthetic values instead of driving the boat:

```
curl -X POST http://<device-ip>/api/debug/gps-inject \
  -H "Content-Type: application/json" \
  -d '{"sog_kn": 5.0, "cog_deg": 90.0, "variation_deg": -8.0}'
```

This endpoint (and the build flag that compiles it in, e.g. `-D DEBUG_GPS_INJECT=1`) exists only in
debug builds — `research.md`/`tasks.md` must confirm it is compiled out of `esp32s3` release builds
so it can never be reachable on a boat. Use it to run a full Stage B GPS-alignment or Stage C swing
end-to-end on the bench, then confirm the resulting offset/deviation against hand-computed expected
values.

## 7. HTTP checks (bench Station mode)

Connect the device to a bench Wi-Fi network (credentials in a git-ignored `secrets.ini`, never
committed) instead of its own Access Point, then:

```
python tools/hil/http_status_check.py --host <device-ip>
python tools/hil/ssid_change_check.py --host <device-ip> --new-ssid "Bench-Test-SSID"
```

`ssid_change_check.py` asserts there is no gap in `[N2K] tx pgn=127250 ok` lines spanning the
SSID-change/AP-restart window (SC-008).

## 8. Optional CAN capture

If a USB-CAN adapter is connected to the same NMEA 2000 segment:

```
python tools/hil/can_capture.py --iface <can-iface> --duration 30 --expect-pgn 127250
```

## 9. Manual verification checklist

Physical accuracy, chartplotter display, and on-water Stage B/C procedures cannot be observed by
the agent (constitution Principle IV) and are tracked in
`checklists/manual-verification.md` (Phase 1 of `tasks.md`), covering SC-001, SC-002, SC-005, and
SC-010.
