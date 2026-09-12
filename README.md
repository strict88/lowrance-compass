# lowrance-compass

Standalone NMEA2000 heading sensor for a chartplotter (Lowrance), built on an
ESP32-S3 with a BNO08x IMU. Reads fused orientation from the BNO08x over I2C
and broadcasts it onto an NMEA2000 (CAN) bus as PGN 127250 (Vessel Heading),
with PGN 127257 (Attitude) planned as an optional extra. A web UI served by
the ESP32 itself will expose IMU calibration status and let the mounting
offset be trimmed at runtime — see `CLAUDE.md` for the full design rationale
and constraints.

## Status: BNO08x reading to console

- [x] PlatformIO project created, board config corrected for the actual
      N16R8 hardware (16MB flash, 8MB octal PSRAM, LittleFS filesystem)
- [x] BNO08x driver integration (I2C, SH-2 rotation vector reads) - prints
      heading + fused-orientation calibration accuracy to the serial console
      only; no NMEA2000/CAN, NVS, or web UI yet
- [ ] TWAI/CAN transceiver wiring and NMEA2000 stack integration
- [ ] PGN 127250 (Vessel Heading) encode + transmit
- [ ] PGN 127257 (Attitude) encode + transmit (optional)
- [ ] Mounting offset (heading trim), persisted to NVS
- [ ] Magnetic calibration status readout + Save/Reset DCD commands
- [ ] Web UI (ESPAsyncWebServer, assets on LittleFS)
- [ ] Feature-flag config layer (`src/config.h/.cpp`, backed by NVS)

`src/main.cpp` now just wires up `src/imu/` and prints heading/calibration
to the serial console. The `src/n2k/` and `src/web/` layout described in
`CLAUDE.md` has not been created yet, and this hasn't been verified on real
hardware (no bench access) — flash and check via `pio device monitor`
before trusting the output.

## Hardware

- MCU: ESP32-S3-DevKitC-1 (N16R8 — 16MB flash / 8MB PSRAM)
- IMU: BNO08x (SH-2 sensor hub), I2C
- CAN transceiver: VP230 (SN65HVD230-compatible), via ESP32-S3 built-in TWAI
- Bus: NMEA2000 (250 kbit/s, 29-bit extended CAN IDs)

Full pinout and hardware notes are in `CLAUDE.md`.

## Build / flash

```
pio run -e esp32-s3-devkitc-1
pio run -t upload
pio run -t uploadfs      # when web UI assets (LittleFS) change
pio device monitor
```

Note: no hardware-in-the-loop testing has been done — flashing/bench
verification is pending.

## Docs

See `CLAUDE.md` for hardware pinout, calibration design, target PGNs,
library choices, and architecture conventions.
