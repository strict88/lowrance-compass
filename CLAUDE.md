# NMEA2000 Heading Sensor (ESP32-S3)

## What this is
A standalone compass/heading sensor that reads orientation from a BNO08x IMU
and transmits it onto an NMEA2000 bus (PGN 127250 Vessel Heading, and
optionally 127257 Attitude / 127258 Magnetic Variation) for consumption by a
chartplotter. Includes a web UI (served by the ESP32 itself) for toggling
features on/off at runtime.

## Hardware
- MCU: ESP32-S3-DevKitC-1 (N16R8 — 16MB flash / 8MB PSRAM)
- IMU: BNO08x (SH-2 sensor hub — fused orientation, don't hand-roll sensor
  fusion, use the chip's rotation vector output)
- CAN transceiver: VP230 (3.3V, SN65HVD230-compatible) driven by the ESP32-S3
  built-in TWAI controller
- Bus: NMEA2000 (250 kbit/s, 11-bit... actually 29-bit extended CAN IDs)

### Pinout (source of truth — update code to match if this changes)
| Signal      | ESP32-S3 GPIO | Note |
|-------------|---------------|------|
| TWAI TX     | GPIO 4        | |
| TWAI RX     | GPIO 5        | |
| BNO08X SDA  | GPIO 8        | I2C0 |
| BNO08X SCL  | GPIO 9        | I2C0 |
| BNO08X INT  | GPIO 15       | active-low, enable pullup |
| BNO08X RST  | GPIO 16       | active-low |

Avoided: GPIO 0/3/45/46 (strapping pins), GPIO 19/20 (native USB D-/D+),
GPIO 33–37 (reserved for octal PSRAM on the N16R8 variant — do not reuse
these even though they may appear free on the schematic).

### IMU interface: I2C (decided)
Using full SH-2 protocol over I2C, not UART-RVC. Reason: UART-RVC is
fixed-rate, output-only (yaw/pitch/roll only) with no calibration accuracy
feedback and no tare/save-calibration commands. I2C gives access to the
Calibration Status report and persistent calibration (Save DCD), which the
web UI will expose. SPI wasn't necessary — I2C bandwidth is plenty for a
single sensor at heading-appropriate update rates (10–20 Hz).

## Git Workflow
- Don't commit and push. Do not create feature branches for
  individual features/changes — this is a solo project, no PR workflow
  needed right now.
- Still use clear, atomic commits (one logical change per commit) even
  though everything lands on main directly.

## Calibration (manual, via web UI)
Two distinct things — don't conflate them:

1. **Magnetic (hard/soft-iron) calibration** — handled by the BNO08x's own
   dynamic calibration algorithm; we don't compute offsets ourselves. Our
   job is to expose it:
   - Poll/subscribe to calibration accuracy (per-sensor, 0=unreliable to
     3=high) and show it live in the UI so the user knows when to stop
     rotating the unit.
   - "Save calibration" button → send the Save DCD (Dynamic Calibration
     Data) command so it persists across power cycles.
   - "Reset calibration" button → clear DCD, start over.
   - Do not attempt custom magnetometer calibration math — the chip already
     does this; duplicating it is redundant and likely worse.

2. **Mounting offset (heading trim)** — independent of magnetic calibration.
   Corrects for the BNO08x's physical mounting angle vs. the vessel's bow.
   - A single signed offset (degrees), stored in NVS, applied to the raw
     heading before it's written into PGN 127250.
   - UI should offer "Set current heading as 0°/North" as the primary
     workflow (point the boat at a known heading, press button) rather than
     requiring the user to type a numeric offset.

## Target PGNs
- **127250 – Vessel Heading** (primary output). Reference = Magnetic (the
  BNO08x rotation vector is magnetometer-fused, so this is magnetic heading,
  not true — don't set Reference = True unless a variation source is added).
- **127257 – Attitude** (pitch/roll, optional but cheap since the IMU
  provides it already).
- 127258 (Magnetic Variation) intentionally not sent yet — no variation
  source exists on this device. Revisit if variation is later read from
  another N2K source (e.g. a GPS PGN) and re-broadcast/cached here.

## Libraries (PlatformIO deps)
- `ttlappalainen/NMEA2000` + `ttlappalainen/NMEA2000_esp32` (TWAI driver glue)
- BNO08x driver: Adafruit_BNO08x or SparkFun BNO08x (SH-2 based) — pick one
  and don't mix API styles
- Web UI: ESPAsyncWebServer + AsyncTCP, assets served from LittleFS
- Config/feature flags: stored in NVS (Preferences library), not hardcoded

## Build / flash
```
pio run -e esp32-s3-devkitc-1
pio run -t upload
pio run -t uploadfs      # when web UI assets (LittleFS) change
pio device monitor
```

## Architecture / conventions
- `src/main.cpp` — setup/loop wiring only, no business logic
- `src/n2k/` — PGN encode/decode, NMEA2000 message handling
- `src/imu/` — BNO08x init, read, calibration status
- `src/web/` — HTTP handlers, WebSocket (if used) for live status
- `src/config.cpp` / `config.h` — feature flags persisted to NVS, single
  source of truth for what's enabled/disabled
- Feature toggles from the web UI must be read at the point of use (or via a
  cached struct updated on change) — don't gate features only at startup,
  since the whole point is runtime enable/disable

## Web UI feature toggles
- Each toggle should map to one clearly named flag in `config.h`
  (e.g. `enableAttitudePGN`, `enableVarianceCorrection`, `enableWebLogging`)
- Persist changes immediately to NVS; don't require a reboot unless a toggle
  genuinely requires re-init of a peripheral (say so explicitly if it does)
- Keep the web UI on a separate core/task from CAN transmission if using
  FreeRTOS tasks — don't let HTTP handling introduce jitter into PGN timing

## Known constraints (important — read before assuming something works)
- I can't flash or bench-test this — I don't have hardware access. Flag
  anything uncertain (register values, exact PGN byte layout/scaling,
  TWAI timing config) instead of guessing confidently.
- NMEA2000 PGN field encoding (scaling factors, byte order, SID usage) is
  easy to get subtly wrong — check against the NMEA2000 library's own PGN
  definitions or the PGN spec before writing custom encode code.
- BNO08x report intervals and calibration status behave differently between
  I2C and UART-RVC — confirm which interface is in use before touching timing
  code.
- ESP32-S3 has one TWAI controller — no bus redundancy at the hardware level.
- Don't block the main loop; CAN frame transmission and IMU reads should stay
  non-blocking so heading updates stay timely on the bus.

## Not yet decided / TODO
- [ ] Magnetic variation source, if true heading is ever wanted
- [ ] UI flow detail: guided calibration wizard vs. simple live-accuracy
      readout + save/reset buttons