# Research: Calibration, Guided Setup & Settings

Every decision below was checked against current (September 2026) releases, not assumed, per the
constitution's "no floating versions" rule. Where a project has no formal tagged releases, an exact
commit SHA is pinned instead — the same "no floating versions" intent applied to an untagged repo.

## 1. PlatformIO platform (Arduino-ESP32 core)

- **Decision**: `pioarduino/platform-espressif32`, not the official `platformio/platform-espressif32`.
- **Pin**: `platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.311/platform-espressif32.zip`
  → bundles Arduino-ESP32 core **3.3.11** / ESP-IDF **5.5.5**.
- **Rationale**: The official platform's latest release still ships Arduino core 2.0.17 / IDF 4.4.7
  — stale, and IDF 4.4.x's legacy TWAI driver naming and S3 gaps make it the wrong base for this
  hardware (see §3). pioarduino tracks upstream `espressif/arduino-esp32` closely and is the
  de-facto community path to Arduino core 3.x today.
- **Alternatives rejected**: Official `platformio/platform-espressif32` (blocks IDF 5.x TWAI API,
  see §3). Pure ESP-IDF without Arduino (out of scope per the constitution's default).
- **Caveat**: Re-verify the exact release-asset filename still resolves at that tag before pinning
  it in `platformio.ini` — GitHub release-asset naming can drift between tags.
- **Sources**: [pioarduino releases](https://github.com/pioarduino/platform-espressif32/releases),
  [official platform releases](https://github.com/platformio/platform-espressif32/releases),
  [pioarduino repo](https://github.com/pioarduino/platform-espressif32)

## 2. BNO08x (SH-2) library

- **Decision**: Keep `adafruit/Adafruit BNO08x` (already a dependency in this repo's
  `platformio.ini`; only needs pinning).
- **Pin**: `adafruit/Adafruit BNO08x @ 1.2.7`.
- **Rationale**: Adafruit_BNO08x bundles the full Hillcrest `sh2` C layer and exposes `sh2.h`
  publicly, so `sh2_setCalConfig(sensors)`, `sh2_saveDcdNow()`, `sh2_setTareNow(axes, basis)`, and
  `sh2_persistTare()` are all directly callable — every capability this plan needs (dynamic
  calibration config, save-DCD, tare) is present, at the C-API level. Per-sensor accuracy (0-3)
  comes through `sh2_SensorValue_t.status` on each accel/gyro/mag/rotation-vector report, no extra
  call needed. Keeping it avoids reworking an existing, already-wired dependency for no capability
  gain.
- **Alternatives considered**: `sparkfun/SparkFun BNO08x Arduino Library` (latest v1.0.6) — nicer
  high-level wrappers (`setCalibrationConfig()`, `tareNow()`, `saveTare()`, `saveCalibration()`)
  over the *same* underlying sh2 calls. Rejected: ergonomics-only difference, not worth ripping out
  the existing dependency.
- **Caveat**: No ESP32-S3-specific fixes are called out in Adafruit's recent changelog (only
  classic-ESP32 I2C-reset fixes in 1.2.2/1.2.3) — validate I2C reliability on the actual SDA=8/
  SCL=9 wiring during Phase 0/1 bring-up; fall back to SparkFun's library only if a concrete
  Adafruit defect surfaces on this hardware.
- **Sources**: [Adafruit_BNO08x releases](https://github.com/adafruit/Adafruit_BNO08x/releases),
  [sh2.h](https://github.com/adafruit/Adafruit_BNO08x/blob/master/src/sh2.h),
  [SparkFun BNO08x releases](https://github.com/sparkfun/SparkFun_BNO08x_Arduino_Library/releases)

## 3. NMEA 2000 stack and ESP32-S3 TWAI driver

- **Decision**: `ttlappalainen/NMEA2000` core library (message/PGN layer, transport-agnostic) +
  **a project-owned `tNMEA2000` subclass** written directly against ESP-IDF's new node-based TWAI
  driver (`esp_twai.h` / `esp_twai_onchip.h`, `twai_node_*` functions). Do **not** depend on
  `sergei/NMEA2000_esp32_twai` or `ktand/NMEA2000_esp32_twai`, and do not use `ttlappalainen`'s own
  `NMEA2000_esp32` bridge.
- **Pin**: `ttlappalainen/NMEA2000` at commit `5b7b9fc3ccc18e30ebfba92da6486cffc625159` (master,
  2025-12-18 — the project has no formal tagged releases, so the commit SHA *is* the pin; verify
  with `git ls-remote` immediately before adding it to `platformio.ini`, since research and
  implementation happen at different times).
- **Rationale — this is the decisive finding of this research pass**: ESP-IDF 5.5.x (what
  pioarduino 55.03.311 ships) has **deprecated the legacy `driver/twai.h` global-driver API** (it
  now emits a deprecation warning) in favor of the new node-based `esp_twai.h` API
  (`twai_new_node_onchip()`, `twai_node_enable()`, `twai_node_transmit()`, `twai_node_recover()` for
  bus-off recovery, `twai_node_get_info()` for tx/rx error counters, an `enable_self_test` flag for
  no-ACK bench mode). Since the constitution requires **zero compiler warnings**, using the legacy
  API is a non-starter. Both community bridges target the legacy API style and are effectively
  abandoned for S3: `sergei/NMEA2000_esp32_twai` has 3 commits and defaults to GPIO32/34 — which
  fall inside *this* board's reserved 26-37 PSRAM range, confirming it targets classic ESP32, not
  S3; `ktand/NMEA2000_esp32_twai` has 21 commits, no tags, and no documented bus-off/error-counter
  handling. `ttlappalainen`'s own `NMEA2000_esp32` bridge is confirmed broken on S2/S3/C3 in that
  repo's own issue tracker. Writing a project-owned subclass directly on the new node API is more
  upfront work than adopting a library, but it is the only option that is both S3-correct and
  warning-clean — directly satisfying constitution Principle II's requirement that "S3 compatibility
  MUST be verified before adoption" (it doesn't pass for any existing bridge).
- **Alternatives rejected**: `NMEA2000_esp32` (broken on S3 per upstream issue #25); both community
  TWAI bridges (unmaintained, legacy-API-shaped, one literally targets the wrong chip's pins).
- **Caveat / carries into plan.md Risks**: This turns "wire up a library" into a from-scratch
  driver-integration task needing dedicated design for bus-off recovery, error-passive detection,
  and self-test/no-ACK bench mode, all using the new node API's callback-based event model
  (`twai_node_register_event_callbacks`) rather than the polling/alerts model most NMEA2000 examples
  online assume. `tasks.md` must size this as its own body of work, not a thin wrapper.
- **Sources**: [NMEA2000_esp32 issue #25](https://github.com/ttlappalainen/NMEA2000_esp32/issues/25),
  [sergei/NMEA2000_esp32_twai](https://github.com/sergei/NMEA2000_esp32_twai),
  [ktand/NMEA2000_esp32_twai](https://github.com/ktand/NMEA2000_esp32_twai),
  [ESP-IDF 5.5 TWAI docs](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/peripherals/twai.html)

## 4. Async web server (WebSocket + REST)

- **Decision**: `ESP32Async/ESPAsyncWebServer` + `ESP32Async/AsyncTCP` — the actively-maintained
  continuation of the archived `me-no-dev` originals.
- **Pin**: `ESP32Async/ESPAsyncWebServer @ 3.12.1`, `ESP32Async/AsyncTCP @ 3.5.0`.
- **Rationale**: Community-recognized successor fork (used by ESPHome and others), WebSocket
  explicitly supported, recently refactored for throughput (up to 20-60 msg/s/client per its own
  v3.10.0 notes) — comfortably covers this feature's bounded broadcast rates
  (`data-model.md` §1.5).
- **Alternatives considered**: Original `me-no-dev/ESPAsyncWebServer` — unmaintained, rejected.
- **Caveat**: AsyncTCP v3.5.0 changed `abort()` to run synchronously in the caller's task context —
  relevant when writing `AppTask`'s WebSocket-disconnect handling, since it changes which task
  context runs cleanup code; note this for the relevant `tasks.md` item.
- **Sources**: [ESPAsyncWebServer releases](https://github.com/ESP32Async/ESPAsyncWebServer/releases),
  [AsyncTCP releases](https://github.com/ESP32Async/AsyncTCP/releases)

## 5. JSON

- **Decision / Pin**: `bblanchon/ArduinoJson @ 7.4.3`.
- **Rationale**: Latest v7.x; this specific version is a security-fix release (buffer overrun in
  `as<T>()` when parsing long float strings into a numeric type) — pin at-or-above it rather than
  an older 7.x.
- **Caveat**: Re-check for a newer 7.4.x patch at implementation time.
- **Sources**: [ArduinoJson releases](https://github.com/bblanchon/ArduinoJson/releases),
  [security advisory](https://arduinojson.org/news/2026/03/02/security-update/)

## 6. Test framework

- **Decision**: PlatformIO's bundled Unity test runner (`test_framework = unity`, the default) for
  both `pio test -e native` and `pio test -e esp32s3_test`. No explicit `lib_deps` pin needed —
  it's managed by PlatformIO core itself, which auto-generates `unity_config.h` when absent.
- **Source**: [PlatformIO Unity docs](https://docs.platformio.org/en/stable/advanced/unit-testing/frameworks/unity.html)

## 7. Persistence approach

- **Decision**: ESP32 NVS via the Arduino `Preferences` API for every record in `data-model.md` §1
  (`SensorCalibrationProfile` metadata, `InstallationAlignment`, `DeviationCorrection`,
  `NetworkSettings`), each in its own `Preferences` namespace, envelope-wrapped with
  `schema_version` + `crc32` as specified there.
- **Rationale**: Matches the constitution's explicit "NVS or LittleFS" guidance for user settings
  and calibration data; NVS's own key-value wear-leveling plus this feature's write-then-verify-
  then-commit envelope together satisfy the "power loss never leaves a half-written record"
  requirement (FR-045/046) without hand-rolling a filesystem-level A/B scheme.
- **Alternatives considered**: A dedicated LittleFS file per record with manual A/B slot files —
  rejected as unnecessary complexity when NVS's atomic per-key commit plus an application-level CRC
  already meets the same guarantee.

## 8. Board configuration & partition table

- **Decision**: `board_build.flash_size = 16MB`, `board_build.psram_type = qio_opi`(memory type)/
  `opi`(psram type, matching the existing `board_build.psram_type = opi` already in this repo's
  `platformio.ini`), USB CDC on boot enabled, LittleFS filesystem, and a custom partition table
  replacing the current `default_16MB.csv` (which has no OTA app slots):

  | Name | Type | SubType | Offset | Size | Notes |
  |---|---|---|---|---|---|
  | `nvs` | data | nvs | `0x9000` | `0x5000` (20 KiB) | Settings + calibration records (§7). |
  | `otadata` | data | ota | `0xe000` | `0x2000` (8 KiB) | OTA slot-select pointer. |
  | `app0` | app | ota_0 | `0x10000` | `0x300000` (3 MiB) | Constitution Principle VII dual-partition OTA. |
  | `app1` | app | ota_1 | `0x310000` | `0x300000` (3 MiB) | |
  | `littlefs` | data | spiffs | `0x610000` | `0x9F0000` (~9.94 MiB) | Web UI assets + guide content, generous headroom. |

  Total: `0x9000` (bootloader/partition-table region, outside this table) + the five partitions
  above = exactly `0x1000000` (16 MiB).
- **Rationale**: 3 MiB per OTA app slot comfortably covers an Arduino/ESP-IDF firmware image with
  this feature's scope (web server, NMEA2000, BNO08x, JSON) with room to grow; the remainder to
  LittleFS is generous for offline UI assets (HTML/CSS/JS/inline-SVG guide content, all
  minified+gzipped per the plan's build step) plus the guide content file.
- **Alternatives considered**: Reusing `default_16MB.csv` — rejected outright, it has no second OTA
  slot, violating constitution Principle VII. A smaller (e.g. 2 MiB) app slot — rejected as too
  tight a margin for a NMEA2000 + async-web + BNO08x firmware image without profiling first.

## 9. UI build tooling

- **Decision**: Vanilla HTML/CSS/JS (no framework, no CDN — constitution Principle VI), inline SVG
  for illustrations, assets minified and gzipped by a PlatformIO `extra_scripts` pre-build Python
  step before being written into the LittleFS image (`pio run -t buildfs` / automatic filesystem
  build).
- **Rationale**: Matches the constitution's "lightweight and dependency-free" UI requirement
  directly; a pre-build script keeps minification out of the runtime firmware and out of version
  control (source files stay readable, built/gzipped output is generated).
- **Alternatives considered**: Committing pre-minified assets directly — rejected, makes UI source
  diffs unreadable in review.

## 10. Host-side tooling

- **Decision**: Python 3 scripts under `tools/hil/`: `pyserial` for serial capture/assertions,
  `requests` for HTTP API checks, `python-can` (optional, only exercised when a USB-CAN adapter is
  present) for real-frame CAN capture — exactly as specified in this feature's plan input, no
  alternative evaluated (these are standard, unambiguous choices for this job).
